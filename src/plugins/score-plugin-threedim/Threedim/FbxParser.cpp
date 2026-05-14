#include "FbxParser.hpp"

#include "TangentUtils.hpp"

#include <ossia/detail/hash.hpp>

#include <ufbx.h>

#include <QQuaternion>

#include <cmath>
#include <cstring>
#include <unordered_map>

namespace Threedim
{

// Transform a position by a ufbx 3x4 matrix (double -> float)
static void transform_point(
    const ufbx_matrix& m, const ufbx_vec3& v, float& ox, float& oy, float& oz)
{
  ox = float(m.m00 * v.x + m.m01 * v.y + m.m02 * v.z + m.m03);
  oy = float(m.m10 * v.x + m.m11 * v.y + m.m12 * v.z + m.m13);
  oz = float(m.m20 * v.x + m.m21 * v.y + m.m22 * v.z + m.m23);
}

// Transform a direction by a ufbx 3x4 matrix (no translation), then normalize
static void transform_normal(
    const ufbx_matrix& m, const ufbx_vec3& v, float& ox, float& oy, float& oz)
{
  float rx = float(m.m00 * v.x + m.m01 * v.y + m.m02 * v.z);
  float ry = float(m.m10 * v.x + m.m11 * v.y + m.m12 * v.z);
  float rz = float(m.m20 * v.x + m.m21 * v.y + m.m22 * v.z);
  float len = std::sqrt(rx * rx + ry * ry + rz * rz);
  if(len > 1e-8f)
  {
    float inv = 1.0f / len;
    ox = rx * inv; oy = ry * inv; oz = rz * inv;
  }
  else
  {
    ox = 0.0f; oy = 1.0f; oz = 0.0f;
  }
}

// =============================================================================
// Scene extractor — builds FbxParser::m_scene_nodes (hierarchical) using the
// node's local_transform (NOT pre-transformed to world). Vertex data lives in
// per-attribute shared buffers owned by ScenePart.
// =============================================================================
struct FbxSceneExtractor
{
  std::vector<FbxParser::SceneNode>& nodes;
  std::vector<std::shared_ptr<ossia::material_component>>& materials;
  std::shared_ptr<ossia::skeleton_component>& skeleton;
  std::unordered_map<const ufbx_material*, int> material_index;
  // bone_node → joint index within the global skeleton.
  std::unordered_map<const ufbx_node*, int> joint_index_of;
  std::vector<uint32_t> tri_indices;

  // Return the joint index for a ufbx bone node, registering a new entry in
  // the global skeleton on first sight. Parent chain is resolved later in a
  // second pass (link_joint_parents).
  int register_joint(const ufbx_node* bone)
  {
    if(!bone)
      return -1;
    auto it = joint_index_of.find(bone);
    if(it != joint_index_of.end())
      return it->second;

    if(!skeleton)
      skeleton = std::make_shared<ossia::skeleton_component>();

    ossia::skeleton_joint j;
    j.name = std::string(bone->name.data, bone->name.length);

    // Local TRS from the bone node itself.
    const auto& lt = bone->local_transform;
    j.translation[0] = float(lt.translation.x);
    j.translation[1] = float(lt.translation.y);
    j.translation[2] = float(lt.translation.z);
    j.rotation[0] = float(lt.rotation.x);
    j.rotation[1] = float(lt.rotation.y);
    j.rotation[2] = float(lt.rotation.z);
    j.rotation[3] = float(lt.rotation.w);
    j.scale[0] = float(lt.scale.x);
    j.scale[1] = float(lt.scale.y);
    j.scale[2] = float(lt.scale.z);

    // Parent linked later. Identity IBM as placeholder; cluster fills it in.
    j.parent_index = -1;
    for(int k = 0; k < 16; ++k)
      j.inverse_bind_matrix[k] = (k % 5 == 0) ? 1.f : 0.f;

    const int idx = (int)skeleton->joints.size();
    skeleton->joints.push_back(j);
    joint_index_of.emplace(bone, idx);
    return idx;
  }

  // After all bones are registered, fill in parent_index for each joint by
  // walking the ufbx parent chain until we find another registered bone.
  void link_joint_parents()
  {
    if(!skeleton)
      return;
    for(auto& [node, idx] : joint_index_of)
    {
      const ufbx_node* p = node->parent;
      while(p)
      {
        auto it = joint_index_of.find(p);
        if(it != joint_index_of.end())
        {
          skeleton->joints[idx].parent_index = it->second;
          break;
        }
        p = p->parent;
      }
    }
  }

  // Convert a ufbx_material to a material_component (factors only — Stage 1b).
  // Returns the index in `materials`, registering it on first sight.
  int register_material(const ufbx_material* m)
  {
    if(!m)
      return -1;
    auto it = material_index.find(m);
    if(it != material_index.end())
      return it->second;

    auto mc = std::make_shared<ossia::material_component>();
    mc->tag = std::string(m->name.data, m->name.length);

    // ufbx exposes both classical (Phong/Lambert) and PBR maps. Prefer PBR
    // values when present; fall back to FBX classical fields otherwise.
    const auto& pbr = m->pbr;
    const auto& fbx = m->fbx;

    // Base color
    if(pbr.base_color.has_value)
    {
      mc->base_color_factor[0] = float(pbr.base_color.value_vec4.x);
      mc->base_color_factor[1] = float(pbr.base_color.value_vec4.y);
      mc->base_color_factor[2] = float(pbr.base_color.value_vec4.z);
      mc->base_color_factor[3] = float(pbr.base_color.value_vec4.w);
    }
    else if(fbx.diffuse_color.has_value)
    {
      mc->base_color_factor[0] = float(fbx.diffuse_color.value_vec3.x);
      mc->base_color_factor[1] = float(fbx.diffuse_color.value_vec3.y);
      mc->base_color_factor[2] = float(fbx.diffuse_color.value_vec3.z);
      mc->base_color_factor[3] = 1.0f;
    }

    // Apply scalar diffuse factor as multiplier on RGB if present.
    if(pbr.base_factor.has_value)
    {
      const float k = float(pbr.base_factor.value_real);
      mc->base_color_factor[0] *= k;
      mc->base_color_factor[1] *= k;
      mc->base_color_factor[2] *= k;
    }

    // Metallic / Roughness
    mc->metallic_factor
        = pbr.metalness.has_value ? float(pbr.metalness.value_real) : 0.0f;
    mc->roughness_factor
        = pbr.roughness.has_value ? float(pbr.roughness.value_real) : 0.5f;

    // Emissive
    if(pbr.emission_color.has_value)
    {
      mc->emissive_factor[0] = float(pbr.emission_color.value_vec3.x);
      mc->emissive_factor[1] = float(pbr.emission_color.value_vec3.y);
      mc->emissive_factor[2] = float(pbr.emission_color.value_vec3.z);
    }
    else if(fbx.emission_color.has_value)
    {
      mc->emissive_factor[0] = float(fbx.emission_color.value_vec3.x);
      mc->emissive_factor[1] = float(fbx.emission_color.value_vec3.y);
      mc->emissive_factor[2] = float(fbx.emission_color.value_vec3.z);
    }
    mc->emissive_strength = pbr.emission_factor.has_value
        ? float(pbr.emission_factor.value_real) : 1.0f;

    // Alpha / opacity
    if(pbr.opacity.has_value)
    {
      const float op = float(pbr.opacity.value_real);
      mc->base_color_factor[3] *= op;
      if(op < 0.999f)
        mc->alpha = ossia::alpha_mode::blend;
    }

    // Material features. Two-sided shading from FBX is uncommon; default false.
    mc->double_sided = false;
    mc->unlit = false;

    // Texture extraction. ufbx_material_map.texture (when non-null) carries
    // either an absolute filename, a relative one (resolved against the FBX
    // file dir), or an embedded blob (`content`). We populate texture_ref
    // with `source` so the renderer's TextureCache can lazily upload on the
    // render thread. The `source` member is never null when a texture is
    // present, even if the file/blob is later unreadable.
    auto fill_texture
        = [](ossia::texture_ref& tr, const ufbx_material_map& map) {
            if(!map.texture)
              return;
            const ufbx_texture* tex = map.texture;
            auto src = std::make_shared<ossia::texture_source>();
            // Prefer absolute filename when present (more robust); fall back
            // to relative + the original "filename" field.
            if(tex->absolute_filename.length > 0)
              src->file_path = std::string(
                  tex->absolute_filename.data, tex->absolute_filename.length);
            else if(tex->filename.length > 0)
              src->file_path = std::string(tex->filename.data, tex->filename.length);
            else if(tex->relative_filename.length > 0)
              src->file_path = std::string(
                  tex->relative_filename.data, tex->relative_filename.length);

            if(tex->content.size > 0)
            {
              auto blob = std::make_shared<std::vector<uint8_t>>(
                  reinterpret_cast<const uint8_t*>(tex->content.data),
                  reinterpret_cast<const uint8_t*>(tex->content.data) + tex->content.size);
              src->embedded_data = blob;
              // ufbx exposes the file extension via the texture name path —
              // best-effort sniff for a MIME hint. The TextureLoader uses
              // QImage::loadFromData with this hint and falls back to header
              // sniffing when empty/wrong.
              auto ext_hint = [&](std::string_view path) -> std::string {
                auto dot = path.rfind('.');
                if(dot == std::string_view::npos)
                  return {};
                std::string e(path.substr(dot + 1));
                for(auto& c : e) c = (char)std::tolower((unsigned char)c);
                if(e == "jpg" || e == "jpeg") return "image/jpeg";
                if(e == "png")               return "image/png";
                if(e == "tga")               return "image/tga";
                if(e == "tif" || e == "tiff") return "image/tiff";
                if(e == "bmp")               return "image/bmp";
                return {};
              };
              src->mime_type = ext_hint(src->file_path);
            }

            // Plan 09 S1: stamp the content hash so the preprocessor's
            // decode cache (Gfx::AssetTable) can skip re-decoding the
            // same image across multiple outputs / scene reloads.
            // Prefer embedded bytes (authoritative) over path (stable
            // fallback when the file is an external reference).
            if(src->embedded_data && !src->embedded_data->empty())
            {
              src->content_hash = ossia::hash_bytes(
                  src->embedded_data->data(),
                  src->embedded_data->size());
            }
            else if(!src->file_path.empty())
            {
              src->content_hash = ossia::hash_bytes(
                  src->file_path.data(), src->file_path.size());
            }

            tr.source = std::move(src);
            tr.texcoord_set = 0;
          };

    fill_texture(mc->base_color_texture,
                 pbr.base_color.texture ? pbr.base_color : fbx.diffuse_color);
    fill_texture(mc->metallic_roughness_texture, pbr.metalness);
    fill_texture(mc->normal_texture,
                 pbr.normal_map.texture ? pbr.normal_map : fbx.normal_map);
    fill_texture(mc->occlusion_texture, pbr.ambient_occlusion);
    fill_texture(mc->emissive_texture,
                 pbr.emission_color.texture ? pbr.emission_color : fbx.emission_color);

    // --- OpenPBR / Arnold StandardSurface extensions --------------------
    // ufbx exposes the full Arnold-family PBR parameter set (coat / sheen
    // / transmission / subsurface / thin-film / anisotropic specular) on
    // ufbx_material_pbr_maps — the same fields OpenPBR aggregates under
    // its coat / fuzz / transmission / subsurface / thin-film lobes. The
    // FBX PBR extension (Autodesk Standard Surface) is the predecessor of
    // OpenPBR, so the mapping is 1:1 name-wise.
    //
    // Each `ufbx_material_map.has_value` tells us whether the DCC
    // actually wrote that channel; if not we leave the material_component
    // field at its spec default.

    auto scalar = [](const ufbx_material_map& map, float fallback) -> float {
      return map.has_value ? float(map.value_real) : fallback;
    };
    auto color3 = [](const ufbx_material_map& map, float (&out)[3],
                     float fx, float fy, float fz) {
      if(map.has_value)
      {
        out[0] = float(map.value_vec3.x);
        out[1] = float(map.value_vec3.y);
        out[2] = float(map.value_vec3.z);
      }
      else
      {
        out[0] = fx; out[1] = fy; out[2] = fz;
      }
    };

    // Coat (KHR_materials_clearcoat equivalent).
    mc->clearcoat.factor = scalar(pbr.coat_factor, 0.0f);
    mc->clearcoat.roughness_factor = scalar(pbr.coat_roughness, 0.0f);
    fill_texture(mc->clearcoat.texture,           pbr.coat_factor);
    fill_texture(mc->clearcoat.roughness_texture, pbr.coat_roughness);
    fill_texture(mc->clearcoat.normal_texture,    pbr.coat_normal);

    // Sheen (fuzz in OpenPBR; KHR_materials_sheen).
    mc->sheen.roughness_factor = scalar(pbr.sheen_roughness, 0.0f);
    color3(pbr.sheen_color, mc->sheen.color_factor, 0.f, 0.f, 0.f);
    fill_texture(mc->sheen.color_texture,     pbr.sheen_color);
    fill_texture(mc->sheen.roughness_texture, pbr.sheen_roughness);

    // Transmission (KHR_materials_transmission). The FBX path tracks
    // thick-walled volume via transmission_depth / scatter / dispersion
    // which we don't carry yet on material_component (see usd-openpbr
    // analysis — volume-depth / scatter / dispersion are listed as the
    // missing fields for full OpenPBR coverage).
    mc->transmission.factor = scalar(pbr.transmission_factor, 0.0f);
    fill_texture(mc->transmission.texture, pbr.transmission_factor);

    // Volume (KHR_materials_volume) — attenuation color ≈ transmission_color.
    // ufbx has no direct thicknessFactor; infer from transmission_depth.
    mc->volume.thickness_factor = scalar(pbr.transmission_depth, 0.0f);
    color3(
        pbr.transmission_color, mc->volume.attenuation_color, 1.f, 1.f, 1.f);

    // Specular (KHR_materials_specular) — Arnold specular_factor +
    // specular_color; anisotropy separately.
    mc->specular.factor = scalar(pbr.specular_factor, 1.0f);
    color3(pbr.specular_color, mc->specular.color_factor, 1.f, 1.f, 1.f);
    fill_texture(mc->specular.texture,       pbr.specular_factor);
    fill_texture(mc->specular.color_texture, pbr.specular_color);

    // IOR (KHR_materials_ior). Falls back to the spec default 1.5 when
    // the FBX didn't write one.
    mc->ior = scalar(pbr.specular_ior, 1.5f);

    // Anisotropy (KHR_materials_anisotropy). ufbx splits anisotropy
    // magnitude (specular_anisotropy) and rotation (specular_rotation).
    mc->anisotropy.strength = scalar(pbr.specular_anisotropy, 0.0f);
    mc->anisotropy.rotation = scalar(pbr.specular_rotation, 0.0f);
    fill_texture(mc->anisotropy.texture, pbr.specular_anisotropy);

    // Iridescence (KHR_materials_iridescence). ufbx's thin_film_*
    // covers the same physics; min == max when ufbx provides only a
    // single thickness value.
    mc->iridescence.factor = scalar(pbr.thin_film_factor, 0.0f);
    const float tf_thickness = scalar(pbr.thin_film_thickness, 400.0f);
    mc->iridescence.thickness_min = tf_thickness;
    mc->iridescence.thickness_max = tf_thickness;
    mc->iridescence.ior = scalar(pbr.thin_film_ior, 1.3f);
    fill_texture(mc->iridescence.texture, pbr.thin_film_factor);

    // Subsurface as diffuse_transmission approximation. OpenPBR-style
    // subsurface fields (weight / color / radius) aren't on our
    // material_component yet, but we map the scalar factor +
    // subsurface_color into diffuse_transmission as the closest
    // available representation so the glTF-side KHR_materials_diffuse_
    // transmission and FBX-side subsurface_factor land in the same slot.
    mc->diffuse_transmission.factor = scalar(pbr.subsurface_factor, 0.0f);
    color3(
        pbr.subsurface_color, mc->diffuse_transmission.color_factor,
        1.f, 1.f, 1.f);
    fill_texture(mc->diffuse_transmission.texture,       pbr.subsurface_factor);
    fill_texture(mc->diffuse_transmission.color_texture, pbr.subsurface_color);

    // Thin-walled flag — Arnold exposes this as a material feature on
    // the FBX side; mirror it to material_component for consumer
    // shaders that want to switch back-side transmission on / off.
    if(m->features.thin_walled.enabled)
    {
      // No dedicated `thin_walled` bool on material_component today;
      // surface it via the generic property map so downstream shaders
      // can opt-in. Key kept stable to match OpenPBR_ResolvedInputs
      // field name.
      mc->properties["thin_walled"] = true;
    }

    // Stable id — deterministic within this FBX load (keyed on the ufbx
    // material's element_id when available, else the running index).
    // Re-reads of the same asset may still mint different ids, but
    // within-session fingerprinting stays pointer-independent.
    mc->stable_id = (m && m->element.element_id)
                        ? (uint64_t)m->element.element_id
                        : ossia::mint_stable_id();
    const int idx = (int)materials.size();
    materials.push_back(mc);
    material_index.emplace(m, idx);
    return idx;
  }

  // Pull a single attribute stream into a freshly-allocated shared buffer.
  // `floats_per_vertex` controls stride. The lambda is called per vertex with
  // (dst_floats, source_index_in_mesh).
  template <typename Read>
  static std::shared_ptr<std::vector<float>> extract_attribute(
      const ufbx_mesh* umesh, const ufbx_mesh_part& part,
      int floats_per_vertex, std::vector<uint32_t>& tris,
      Read&& read)
  {
    const int64_t num_verts = int64_t(part.num_triangles) * 3;
    auto out = std::make_shared<std::vector<float>>(size_t(num_verts) * floats_per_vertex);
    float* dst = out->data();
    for(size_t fi = 0; fi < part.num_faces; fi++)
    {
      const uint32_t face_idx = part.face_indices.data[fi];
      const ufbx_face face = umesh->faces.data[face_idx];
      tris.resize(face.num_indices * 3);
      uint32_t num_tris = ufbx_triangulate_face(tris.data(), tris.size(), umesh, face);
      for(uint32_t ti = 0; ti < num_tris; ti++)
      {
        for(int vi = 0; vi < 3; vi++)
        {
          uint32_t idx = tris[ti * 3 + vi];
          read(dst, idx);
          dst += floats_per_vertex;
        }
      }
    }
    return out;
  }

  // Build a ScenePart for one (mesh, material_part) pair. Vertex data is in
  // mesh-local space — node hierarchy carries the transform.
  FbxParser::ScenePart extract_part(
      const ufbx_node* node, const ufbx_mesh* umesh,
      const ufbx_mesh_part& part)
  {
    FbxParser::ScenePart sp;
    sp.vertex_count = uint32_t(part.num_triangles) * 3;
    if(sp.vertex_count == 0)
      return sp;

    const bool has_normals = umesh->vertex_normal.exists;
    const bool has_uv = umesh->vertex_uv.exists;
    const bool has_colors = umesh->vertex_color.exists;
    const bool has_tangents = umesh->vertex_tangent.exists;

    sp.positions = extract_attribute(
        umesh, part, 3, tri_indices, [umesh](float* dst, uint32_t idx) {
          ufbx_vec3 p = umesh->vertex_position.values.data[
              umesh->vertex_position.indices.data[idx]];
          dst[0] = float(p.x); dst[1] = float(p.y); dst[2] = float(p.z);
        });
    // Local-space AABB for per-draw GPU culling. Walk the just-extracted
    // positions once. ~10 ns/vertex — negligible at load time.
    if(sp.positions && !sp.positions->empty())
      sp.bounds = ossia::compute_aabb_from_positions(
          sp.positions->data(), sp.vertex_count);

    if(has_normals)
    {
      sp.normals = extract_attribute(
          umesh, part, 3, tri_indices, [umesh](float* dst, uint32_t idx) {
            ufbx_vec3 n = umesh->vertex_normal.values.data[
                umesh->vertex_normal.indices.data[idx]];
            float len = float(std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z));
            if(len > 1e-8f)
            {
              float inv = 1.f / len;
              dst[0] = float(n.x) * inv;
              dst[1] = float(n.y) * inv;
              dst[2] = float(n.z) * inv;
            }
            else
            {
              dst[0] = 0.f; dst[1] = 1.f; dst[2] = 0.f;
            }
          });
    }

    if(has_uv)
    {
      sp.texcoords = extract_attribute(
          umesh, part, 2, tri_indices, [umesh](float* dst, uint32_t idx) {
            ufbx_vec2 uv = umesh->vertex_uv.values.data[
                umesh->vertex_uv.indices.data[idx]];
            dst[0] = float(uv.x); dst[1] = float(uv.y);
          });
    }

    if(has_colors)
    {
      sp.colors = extract_attribute(
          umesh, part, 4, tri_indices, [umesh](float* dst, uint32_t idx) {
            ufbx_vec4 c = umesh->vertex_color.values.data[
                umesh->vertex_color.indices.data[idx]];
            dst[0] = float(c.x); dst[1] = float(c.y);
            dst[2] = float(c.z); dst[3] = float(c.w);
          });
    }

    if(has_tangents)
    {
      sp.tangents = extract_attribute(
          umesh, part, 4, tri_indices, [umesh](float* dst, uint32_t idx) {
            ufbx_vec3 t = umesh->vertex_tangent.values.data[
                umesh->vertex_tangent.indices.data[idx]];
            float len = float(std::sqrt(t.x * t.x + t.y * t.y + t.z * t.z));
            if(len > 1e-8f)
            {
              float inv = 1.f / len;
              dst[0] = float(t.x) * inv;
              dst[1] = float(t.y) * inv;
              dst[2] = float(t.z) * inv;
            }
            else
            {
              dst[0] = 1.f; dst[1] = 0.f; dst[2] = 0.f;
            }
            // Compute handedness from bitangent if present
            if(umesh->vertex_bitangent.exists)
            {
              ufbx_vec3 n = umesh->vertex_normal.values.data[
                  umesh->vertex_normal.indices.data[idx]];
              ufbx_vec3 b = umesh->vertex_bitangent.values.data[
                  umesh->vertex_bitangent.indices.data[idx]];
              float cx = float(n.y * t.z - n.z * t.y);
              float cy = float(n.z * t.x - n.x * t.z);
              float cz = float(n.x * t.y - n.y * t.x);
              float d = cx * float(b.x) + cy * float(b.y) + cz * float(b.z);
              dst[3] = d < 0.f ? -1.f : 1.f;
            }
            else
            {
              dst[3] = 1.f;
            }
          });
    }
    else if(has_normals && has_uv)
    {
      // FBX mesh has no TANGENT channel — synthesize tangents from
      // position / normal / UV via mikktspace so normal maps work.
      // Extracted attributes here are already triangle-unindexed
      // (each triangle has 3 unique vertices), so no index buffer is
      // needed and mikktspace's contract is satisfied naturally.
      sp.tangents = Threedim::generate_tangents_mikktspace(
          sp.positions, sp.normals, sp.texcoords,
          /*indices=*/nullptr, sp.vertex_count);
    }

    // Skinning: if the mesh has a skin deformer, pull top-4 (cluster, weight)
    // pairs per vertex. ufbx sorts weights descending, so we can truncate to
    // 4 safely. Joint indices map through register_joint into the global
    // skeleton. The per-triangle expansion mirrors the position walk: one
    // output entry per (face_index, triangulated_vertex).
    if(umesh->skin_deformers.count > 0)
    {
      const ufbx_skin_deformer* skin = umesh->skin_deformers.data[0];

      // Register all clusters' bones up front so register_joint is a plain
      // lookup in the hot per-vertex loop below.
      std::vector<int> cluster_to_joint(skin->clusters.count, -1);
      for(size_t ci = 0; ci < skin->clusters.count; ci++)
      {
        const ufbx_skin_cluster* cl = skin->clusters.data[ci];
        if(!cl || !cl->bone_node)
          continue;
        int j = register_joint(cl->bone_node);
        cluster_to_joint[ci] = j;

        // The cluster's geometry_to_bone IS the inverse-bind matrix (glTF
        // convention): vertices in geometry-local space → bone-local. Store
        // as column-major 4x4 (ufbx_matrix is row-major 3x4; we transpose).
        const ufbx_matrix& m = cl->geometry_to_bone;
        float* ibm = skeleton->joints[j].inverse_bind_matrix;
        // Column 0: (m00, m10, m20, 0), col 1, col 2, col 3 (translation)
        ibm[0] = float(m.m00); ibm[1] = float(m.m10); ibm[2] = float(m.m20); ibm[3] = 0.f;
        ibm[4] = float(m.m01); ibm[5] = float(m.m11); ibm[6] = float(m.m21); ibm[7] = 0.f;
        ibm[8] = float(m.m02); ibm[9] = float(m.m12); ibm[10] = float(m.m22); ibm[11] = 0.f;
        ibm[12] = float(m.m03); ibm[13] = float(m.m13); ibm[14] = float(m.m23); ibm[15] = 1.f;
      }

      // Allocate joints0/weights0 per-triangle-vertex buffers. ufbx indexes
      // skin_vertices by the base vertex (not the triangulated index), so
      // we resolve via umesh->vertex_position.indices — same pattern as the
      // attribute extraction above.
      const int64_t num_verts = int64_t(part.num_triangles) * 3;
      auto joints_buf = std::make_shared<std::vector<uint16_t>>(size_t(num_verts) * 4);
      auto weights_buf = std::make_shared<std::vector<float>>(size_t(num_verts) * 4);
      uint16_t* jdst = joints_buf->data();
      float*    wdst = weights_buf->data();

      for(size_t fi = 0; fi < part.num_faces; fi++)
      {
        const uint32_t face_idx = part.face_indices.data[fi];
        const ufbx_face face = umesh->faces.data[face_idx];
        tri_indices.resize(face.num_indices * 3);
        uint32_t num_tris = ufbx_triangulate_face(
            tri_indices.data(), tri_indices.size(), umesh, face);
        for(uint32_t ti = 0; ti < num_tris; ti++)
        {
          for(int vi = 0; vi < 3; vi++)
          {
            uint32_t idx = tri_indices[ti * 3 + vi];
            uint32_t base_vtx = umesh->vertex_position.indices.data[idx];
            const ufbx_skin_vertex sv = skin->vertices.data[base_vtx];

            // Pick up to 4 weights (already sorted descending by weight).
            float w[4] = {0, 0, 0, 0};
            uint16_t j[4] = {0, 0, 0, 0};
            const uint32_t n = std::min<uint32_t>(sv.num_weights, 4);
            for(uint32_t k = 0; k < n; ++k)
            {
              const ufbx_skin_weight sw = skin->weights.data[sv.weight_begin + k];
              if(sw.cluster_index < cluster_to_joint.size()
                 && cluster_to_joint[sw.cluster_index] >= 0)
              {
                j[k] = uint16_t(cluster_to_joint[sw.cluster_index]);
                w[k] = float(sw.weight);
              }
            }
            // Renormalise — ufbx doesn't guarantee the top-4 sum to 1.
            float sum = w[0] + w[1] + w[2] + w[3];
            if(sum > 1e-6f)
            {
              float inv = 1.f / sum;
              w[0] *= inv; w[1] *= inv; w[2] *= inv; w[3] *= inv;
            }
            jdst[0] = j[0]; jdst[1] = j[1]; jdst[2] = j[2]; jdst[3] = j[3];
            wdst[0] = w[0]; wdst[1] = w[1]; wdst[2] = w[2]; wdst[3] = w[3];
            jdst += 4;
            wdst += 4;
          }
        }
      }

      sp.joints0 = std::move(joints_buf);
      sp.weights0 = std::move(weights_buf);
      sp.skin_joint_count = int(skeleton ? skeleton->joints.size() : 0);
    }

    // Material assignment — prefer the per-instance node->materials list
    // (FBX allows different node instances to override mesh materials), fall
    // back to the mesh's own materials list, then to part.material.
    const ufbx_material* mat = nullptr;
    if(part.index < node->materials.count)
      mat = node->materials.data[part.index];
    if(!mat && part.index < umesh->materials.count)
      mat = umesh->materials.data[part.index];
    sp.material_index = register_material(mat);

    return sp;
  }

  // Convert a ufbx_light to a populated light_component. Caller takes
  // ownership. Returns nullptr if the light isn't representable (e.g. ufbx
  // VOLUME type).
  static std::shared_ptr<ossia::light_component> to_light(const ufbx_light* l)
  {
    if(!l)
      return {};
    auto lc = std::make_shared<ossia::light_component>();
    switch(l->type)
    {
      case UFBX_LIGHT_DIRECTIONAL:
        lc->type = ossia::light_type::directional; break;
      case UFBX_LIGHT_POINT:
        lc->type = ossia::light_type::point; break;
      case UFBX_LIGHT_SPOT:
        lc->type = ossia::light_type::spot; break;
      case UFBX_LIGHT_AREA:
        // ufbx exposes either rectangle or sphere area shape; map the common
        // rect case, fall back to disk for sphere (close enough at v1).
        lc->type = (l->area_shape == UFBX_LIGHT_AREA_SHAPE_RECTANGLE)
            ? ossia::light_type::rect_area
            : ossia::light_type::sphere_area;
        break;
      default: // UFBX_LIGHT_VOLUME and any future types — skip.
        return {};
    }
    switch(l->decay)
    {
      case UFBX_LIGHT_DECAY_NONE:      lc->decay = ossia::light_decay::none; break;
      case UFBX_LIGHT_DECAY_LINEAR:    lc->decay = ossia::light_decay::linear; break;
      case UFBX_LIGHT_DECAY_QUADRATIC: lc->decay = ossia::light_decay::quadratic; break;
      case UFBX_LIGHT_DECAY_CUBIC:     lc->decay = ossia::light_decay::cubic; break;
      default: break;
    }
    lc->color[0] = float(l->color.x);
    lc->color[1] = float(l->color.y);
    lc->color[2] = float(l->color.z);
    lc->intensity = float(l->intensity);
    lc->inner_cone_angle = float(l->inner_angle) * float(M_PI) / 180.f;
    lc->outer_cone_angle = float(l->outer_angle) * float(M_PI) / 180.f;
    lc->shadow.enabled = l->cast_shadows;

    // Range: FBX doesn't expose falloff distance as a first-class
    // ufbx_light field, but the underlying FBX property `FarAttenuationEnd`
    // (the distance past which the light contributes nothing) maps
    // cleanly onto score's `range`. 0 = infinite, which is the ossia
    // light_component convention for "no cutoff."  Read via the generic
    // props accessor since ufbx pins it in `l->props`, not in the
    // ufbx_light struct fields.
    lc->range = float(ufbx_find_real(&l->props, "FarAttenuationEnd", 0.0));

    // Area-light dimensions: FBX has no standard area_width / area_height
    // fields in ufbx_light. Authoring tools encode area size through
    // the node's own scale; we leave lc->width / height / radius at
    // their defaults and let a future shader-side area sampler derive
    // effective dimensions from the node transform when needed.

    // `l->cast_light` (bool) is the "is this light emitting at all"
    // gate in FBX. ossia::light_component has no direct equivalent —
    // a disabled light would be culled upstream (scene_filter by visibility
    // or a dedicated filter). Dropping a non-emitting light here keeps
    // the RawLight arena from accumulating dead slots.
    if(!l->cast_light)
      return {};

    return lc;
  }

  // Convert a ufbx_camera to a camera_component. Field-of-view in ufbx is
  // degrees (vertical for "horizontal" axis); ossia stores radians.
  static std::shared_ptr<ossia::camera_component> to_camera(const ufbx_camera* c)
  {
    if(!c)
      return {};
    auto cc = std::make_shared<ossia::camera_component>();
    cc->projection = (c->projection_mode == UFBX_PROJECTION_MODE_ORTHOGRAPHIC)
        ? ossia::camera_projection::orthographic
        : ossia::camera_projection::perspective;
    cc->yfov = float(c->field_of_view_deg.y) * float(M_PI) / 180.f;
    cc->aspect_ratio = float(c->aspect_ratio > 0 ? c->aspect_ratio : 1.0);
    cc->xmag = float(c->orthographic_size.x);
    cc->ymag = float(c->orthographic_size.y);
    cc->znear = float(c->near_plane);
    cc->zfar  = float(c->far_plane);
    cc->physical.focal_length = float(c->focal_length_mm);
    cc->physical.horizontal_aperture = float(c->aperture_size_inch.x * 25.4);
    cc->physical.vertical_aperture   = float(c->aperture_size_inch.y * 25.4);
    return cc;
  }

  void extract_node(const ufbx_node* node, int parent_index)
  {
    FbxParser::SceneNode sn;
    sn.name = std::string(node->name.data, node->name.length);
    sn.parent_index = parent_index;
    sn.light = to_light(node->light);
    sn.camera = to_camera(node->camera);

    // Decompose local_transform — ufbx already gives us TRS.
    const auto& lt = node->local_transform;
    sn.local_transform.translation[0] = float(lt.translation.x);
    sn.local_transform.translation[1] = float(lt.translation.y);
    sn.local_transform.translation[2] = float(lt.translation.z);
    sn.local_transform.rotation[0] = float(lt.rotation.x);
    sn.local_transform.rotation[1] = float(lt.rotation.y);
    sn.local_transform.rotation[2] = float(lt.rotation.z);
    sn.local_transform.rotation[3] = float(lt.rotation.w);
    sn.local_transform.scale[0] = float(lt.scale.x);
    sn.local_transform.scale[1] = float(lt.scale.y);
    sn.local_transform.scale[2] = float(lt.scale.z);

    // Extract mesh parts if this node holds a mesh.
    if(node->mesh)
    {
      const ufbx_mesh* umesh = node->mesh;
      if(umesh->material_parts.count > 0)
      {
        for(size_t pi = 0; pi < umesh->material_parts.count; pi++)
        {
          auto sp = extract_part(node, umesh, umesh->material_parts.data[pi]);
          if(sp.vertex_count > 0)
            sn.parts.push_back(std::move(sp));
        }
      }
      else
      {
        ufbx_mesh_part whole{};
        whole.num_faces = umesh->num_faces;
        whole.num_triangles = umesh->num_triangles;
        std::vector<uint32_t> all_faces(umesh->num_faces);
        for(size_t i = 0; i < umesh->num_faces; i++)
          all_faces[i] = uint32_t(i);
        whole.face_indices.data = all_faces.data();
        whole.face_indices.count = all_faces.size();
        auto sp = extract_part(node, umesh, whole);
        if(sp.vertex_count > 0)
          sn.parts.push_back(std::move(sp));
      }
    }

    const int self_index = (int)nodes.size();
    nodes.push_back(std::move(sn));

    // Recurse into children.
    for(size_t ci = 0; ci < node->children.count; ci++)
      extract_node(node->children.data[ci], self_index);
  }

  void extract_scene(const ufbx_scene* scene)
  {
    // Skip the synthetic root node; emit its children as actual roots.
    if(!scene->root_node)
      return;
    for(size_t ci = 0; ci < scene->root_node->children.count; ci++)
      extract_node(scene->root_node->children.data[ci], -1);
  }
};

// =============================================================================
// rebuild_scene — walk m_scene_nodes, build hierarchical scene_spec with
// mesh_primitive[] (modern path; ScenePreprocessor handles both this and the
// legacy_geometry path).
// =============================================================================

// Wrap a per-attribute float buffer as a buffer_resource_ptr suitable for
// mesh_primitive::vertex_buffers. The data lifetime is held by the shared
// pointer aliasing — no extra copy.
static ossia::buffer_resource_ptr make_buffer_resource(
    std::shared_ptr<std::vector<float>> floats)
{
  if(!floats || floats->empty())
    return {};
  auto br = std::make_shared<ossia::buffer_resource>();
  ossia::buffer_data bd;
  // Aliasing constructor: the resulting shared_ptr keeps `floats` alive but
  // exposes a `const void*` pointing at the contiguous data.
  bd.data = std::shared_ptr<const void>(floats, floats->data());
  bd.byte_size = int64_t(floats->size() * sizeof(float));
  bd.usage_hint = ossia::buffer_data::usage::vertex_buffer;
  br->resource = std::move(bd);
  br->dirty_index = 1;
  return br;
}

// Build one mesh_primitive from a ScenePart. Each present attribute lives in
// its own buffer (one buffer_index per attribute, one binding per attribute).
static ossia::mesh_primitive part_to_primitive(
    const FbxParser::ScenePart& part,
    const std::vector<std::shared_ptr<ossia::material_component>>& mats)
{
  ossia::mesh_primitive mp;
  mp.stable_id = ossia::mint_stable_id();
  mp.topology = ossia::primitive_topology::triangles;
  mp.index_type = ossia::index_format::none;
  mp.vertex_count = part.vertex_count;
  mp.index_count = 0;
  mp.first_vertex = 0;
  mp.first_index = 0;
  mp.vertex_offset = 0;
  mp.bounds = part.bounds;
  if(part.material_index >= 0
     && std::size_t(part.material_index) < mats.size())
    mp.material = mats[part.material_index];

  uint32_t buffer_idx = 0;
  auto add = [&](std::shared_ptr<std::vector<float>> data, int floats_per_vertex,
                 ossia::attribute_semantic sem, ossia::vertex_format fmt) {
    if(!data || data->empty())
      return;
    mp.vertex_buffers.push_back(make_buffer_resource(std::move(data)));
    ossia::vertex_attribute attr;
    attr.semantic = sem;
    attr.format = fmt;
    attr.buffer_index = buffer_idx;
    attr.byte_offset = 0;
    attr.byte_stride = uint32_t(floats_per_vertex) * sizeof(float);
    attr.rate = ossia::vertex_attribute::input_rate::per_vertex;
    mp.attributes.push_back(attr);
    ++buffer_idx;
  };

  add(part.positions, 3,
      ossia::attribute_semantic::position, ossia::vertex_format::float3);
  add(part.normals, 3,
      ossia::attribute_semantic::normal, ossia::vertex_format::float3);
  add(part.texcoords, 2,
      ossia::attribute_semantic::texcoord0, ossia::vertex_format::float2);
  add(part.colors, 4,
      ossia::attribute_semantic::color0, ossia::vertex_format::float4);
  add(part.tangents, 4,
      ossia::attribute_semantic::tangent, ossia::vertex_format::float4);

  // Skinning attributes. joints0 is uint16x4 (halves per-vertex storage vs
  // uint32x4); weights0 is float4. Only emitted when the mesh has skinning.
  if(part.joints0 && !part.joints0->empty())
  {
    auto joint_br = std::make_shared<ossia::buffer_resource>();
    ossia::buffer_data bd;
    bd.data = std::shared_ptr<const void>(part.joints0, part.joints0->data());
    bd.byte_size = int64_t(part.joints0->size() * sizeof(uint16_t));
    bd.usage_hint = ossia::buffer_data::usage::vertex_buffer;
    joint_br->resource = std::move(bd);
    joint_br->dirty_index = 1;
    mp.vertex_buffers.push_back(joint_br);

    ossia::vertex_attribute attr;
    attr.semantic = ossia::attribute_semantic::joints0;
    attr.format = ossia::vertex_format::uint16x4;
    attr.buffer_index = buffer_idx++;
    attr.byte_offset = 0;
    attr.byte_stride = 4 * sizeof(uint16_t);
    attr.rate = ossia::vertex_attribute::input_rate::per_vertex;
    mp.attributes.push_back(attr);
  }
  if(part.weights0 && !part.weights0->empty())
  {
    mp.vertex_buffers.push_back(make_buffer_resource(part.weights0));
    ossia::vertex_attribute attr;
    attr.semantic = ossia::attribute_semantic::weights0;
    attr.format = ossia::vertex_format::float4;
    attr.buffer_index = buffer_idx++;
    attr.byte_offset = 0;
    attr.byte_stride = 4 * sizeof(float);
    attr.rate = ossia::vertex_attribute::input_rate::per_vertex;
    mp.attributes.push_back(attr);
  }

  return mp;
}

void FbxParser::rebuild_scene()
{
  if(m_scene_nodes.empty())
    return;

  // Allocate scene_node + children list shells in flat arrays first, then
  // wire children using parent_index. Two-pass keeps the code simple and
  // avoids any std::shared_ptr<scene_node> circular-ownership concerns.
  const std::size_t N = m_scene_nodes.size();
  std::vector<std::shared_ptr<ossia::scene_node>> nodes;
  std::vector<std::shared_ptr<std::vector<ossia::scene_payload>>> children_lists;
  nodes.reserve(N);
  children_lists.reserve(N);
  for(std::size_t i = 0; i < N; ++i)
  {
    auto n = std::make_shared<ossia::scene_node>();
    n->name = m_scene_nodes[i].name;
    n->visible = true;
    nodes.push_back(std::move(n));
    children_lists.push_back(
        std::make_shared<std::vector<ossia::scene_payload>>());
  }

  // Per-node payload list: first the local transform (so it applies to all
  // subsequent siblings, matching FlattenVisitor's convention), then the
  // mesh_component (if any). Child nodes are pushed in the second pass.
  for(std::size_t i = 0; i < N; ++i)
  {
    auto& src = m_scene_nodes[i];
    auto& lst = *children_lists[i];

    lst.push_back(src.local_transform);

    if(!src.parts.empty())
    {
      auto mc = std::make_shared<ossia::mesh_component>();
      mc->primitives.reserve(src.parts.size());
      bool any_skinned = false;
      for(const auto& part : src.parts)
      {
        mc->primitives.push_back(part_to_primitive(part, m_materials));
        if(part.skin_joint_count > 0)
          any_skinned = true;
      }
      // Attach the global skeleton when any part of this mesh is skinned.
      if(any_skinned && m_skeleton)
        mc->skin = ossia::skeleton_component_ptr(m_skeleton);
      mc->dirty_index = 1;
      lst.push_back(ossia::mesh_component_ptr(std::move(mc)));
    }
    if(src.light)
      lst.push_back(ossia::light_component_ptr(src.light));
    if(src.camera)
      lst.push_back(ossia::camera_component_ptr(src.camera));
  }

  // Wire children (parent_index references earlier entries).
  for(std::size_t i = 0; i < N; ++i)
  {
    int p = m_scene_nodes[i].parent_index;
    if(p >= 0 && p < int(N))
      children_lists[p]->push_back(ossia::scene_node_ptr(nodes[i]));
  }
  for(std::size_t i = 0; i < N; ++i)
    nodes[i]->children = children_lists[i];

  auto roots = std::make_shared<std::vector<ossia::scene_node_ptr>>();
  for(std::size_t i = 0; i < N; ++i)
    if(m_scene_nodes[i].parent_index < 0)
      roots->push_back(ossia::scene_node_ptr(nodes[i]));

  // Materials: publish the registered list. Const conversion happens via
  // material_component_ptr (shared_ptr<const material_component>).
  auto mat_list = std::make_shared<std::vector<ossia::material_component_ptr>>();
  mat_list->reserve(m_materials.size());
  for(auto& m : m_materials)
    mat_list->push_back(ossia::material_component_ptr(m));

  auto state = std::make_shared<ossia::scene_state>();
  state->roots = std::move(roots);
  state->materials = std::move(mat_list);
  if(m_skeleton && !m_skeleton->joints.empty())
  {
    auto skins = std::make_shared<std::vector<ossia::skeleton_component_ptr>>();
    skins->push_back(ossia::skeleton_component_ptr(m_skeleton));
    state->skeletons = std::move(skins);
  }
  state->version = 1;
  state->dirty_index = 1;

  // AssetLoader wraps m_raw_state in a TRS payload externally; we
  // publish only the raw scene here.
  m_raw_state = std::move(state);
}

std::function<void(FbxParser&)> FbxParser::ins::fbx_t::process(file_type tv)
{
  if(tv.filename.empty())
    return {};

  ufbx_load_opts opts{};
  opts.generate_missing_normals = true;
  opts.normalize_normals = true;
  opts.normalize_tangents = true;

  // Convert to OpenGL coordinate system: +X right, +Y up, +Z front (= -Z forward)
  opts.target_axes.right = UFBX_COORDINATE_AXIS_POSITIVE_X;
  opts.target_axes.up = UFBX_COORDINATE_AXIS_POSITIVE_Y;
  opts.target_axes.front = UFBX_COORDINATE_AXIS_POSITIVE_Z;
  opts.target_unit_meters = 1.0;

  // Bake "geometric transforms" (the non-inherited per-attachment offset) into
  // the vertex data. This means node->geometry_transform is identity afterward
  // and the meshes' vertex positions are in the node's local frame — exactly
  // what we want for the hierarchical scene_spec output.
  opts.geometry_transform_handling = UFBX_GEOMETRY_TRANSFORM_HANDLING_MODIFY_GEOMETRY;
  opts.space_conversion = UFBX_SPACE_CONVERSION_ADJUST_TRANSFORMS;
  opts.use_blender_pbr_material = true;

  ufbx_error error{};
  ufbx_scene* scene = ufbx_load_file(tv.filename.data(), &opts, &error);
  if(!scene)
    return {};

  // Extract hierarchical scene (drives rebuild_scene).
  std::vector<FbxParser::SceneNode> scene_nodes;
  std::vector<std::shared_ptr<ossia::material_component>> materials;
  std::shared_ptr<ossia::skeleton_component> skeleton;
  FbxSceneExtractor scene_ex{scene_nodes, materials, skeleton, {}, {}, {}};
  scene_ex.extract_scene(scene);
  scene_ex.link_joint_parents();

  ufbx_free_scene(scene);

  if(scene_nodes.empty())
    return {};

  return [scene_nodes = std::move(scene_nodes),
          materials = std::move(materials),
          skeleton = std::move(skeleton)](FbxParser& o) mutable {
    std::swap(o.m_scene_nodes, scene_nodes);
    std::swap(o.m_materials, materials);
    o.m_skeleton = std::move(skeleton);
    o.rebuild_scene();
  };
}

}
