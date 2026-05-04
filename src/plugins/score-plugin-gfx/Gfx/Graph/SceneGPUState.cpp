#include <Gfx/Graph/SceneGPUState.hpp>

#include <ossia/detail/hash_map.hpp>

#include <QDebug>
#include <QQuaternion>

#include <cmath>
#include <cstring>

namespace score::gfx
{

static QMatrix4x4 toQMatrix(const ossia::transform3d& t)
{
  // ossia::transform3d::matrix stores column-major data.
  // QMatrix4x4(values, cols, rows) with cols=4, rows=4 reads column-major.
  return QMatrix4x4(t.matrix, 4, 4);
}

static QMatrix4x4 toQMatrix(const ossia::scene_transform& t)
{
  QMatrix4x4 mat;
  mat.translate(t.translation[0], t.translation[1], t.translation[2]);
  mat.rotate(QQuaternion(t.rotation[3], t.rotation[0], t.rotation[1], t.rotation[2]));
  mat.scale(t.scale[0], t.scale[1], t.scale[2]);
  return mat;
}

// packLight removed in task 28c. The Light producer owns a RawLight
// arena slot and writes RawLightData directly in its own update() hook
// (see Threedim/Light.cpp); the preprocessor no longer CPU-composes
// world-space light bytes. Consumer shaders compose direction / position
// on the fly from world_transforms[RawLight.transform_slot].

// ---- mesh_primitive → ossia::geometry ------------------------------------
//
// Builds a transient `ossia::geometry` on the heap that wraps a
// `mesh_primitive`'s buffers and attribute layout. The downstream
// preprocessor copies those handles into its own output, so the converted
// geometry only needs to survive the current flatten pass. CPU-backed
// `buffer_data` flows
// through as `cpu_buffer` (the rendering layer handles the upload); GPU
// handles flow through as `gpu_buffer`.

static decltype(ossia::geometry::attribute::format)
toGeomAttrFormat(ossia::vertex_format f) noexcept
{
  using V = ossia::vertex_format;
  using A = decltype(ossia::geometry::attribute::format);
  switch(f)
  {
    case V::float1:      return ossia::geometry::attribute::float1;
    case V::float2:      return ossia::geometry::attribute::float2;
    case V::float3:      return ossia::geometry::attribute::float3;
    case V::float4:      return ossia::geometry::attribute::float4;
    case V::half1:       return ossia::geometry::attribute::half1;
    case V::half2:       return ossia::geometry::attribute::half2;
    case V::half3:       return ossia::geometry::attribute::half3;
    case V::half4:       return ossia::geometry::attribute::half4;
    case V::unorm8x1:    return ossia::geometry::attribute::unormbyte1;
    case V::unorm8x2:    return ossia::geometry::attribute::unormbyte2;
    case V::unorm8x4:    return ossia::geometry::attribute::unormbyte4;
    case V::uint16x1:    return ossia::geometry::attribute::ushort1;
    case V::uint16x2:    return ossia::geometry::attribute::ushort2;
    case V::uint16x4:    return ossia::geometry::attribute::ushort4;
    case V::sint16x1:    return ossia::geometry::attribute::sshort1;
    case V::sint16x2:    return ossia::geometry::attribute::sshort2;
    case V::sint16x4:    return ossia::geometry::attribute::sshort4;
    case V::uint32x1:    return ossia::geometry::attribute::uint1;
    case V::uint32x2:    return ossia::geometry::attribute::uint2;
    case V::uint32x3:    return ossia::geometry::attribute::uint3;
    case V::uint32x4:    return ossia::geometry::attribute::uint4;
    case V::sint32x1:    return ossia::geometry::attribute::sint1;
    case V::sint32x2:    return ossia::geometry::attribute::sint2;
    case V::sint32x3:    return ossia::geometry::attribute::sint3;
    case V::sint32x4:    return ossia::geometry::attribute::sint4;
    default:             return ossia::geometry::attribute::float3;
  }
}

static auto toGeomTopology(ossia::primitive_topology t) noexcept
{
  using P = ossia::primitive_topology;
  using G = decltype(ossia::geometry::topology);
  switch(t)
  {
    case P::points:         return G::points;
    case P::lines:          return G::lines;
    case P::line_strip:     return G::line_strip;
    case P::triangles:      return G::triangles;
    case P::triangle_strip: return G::triangle_strip;
    case P::triangle_fan:   return G::triangle_fan;
    default:                return G::triangles;
  }
}

static void appendBufferResource(
    ossia::geometry& g, const ossia::buffer_resource& br)
{
  if(auto* cpu = ossia::get_if<ossia::buffer_data>(&br.resource))
  {
    ossia::geometry::cpu_buffer cb;
    // buffer_data::data is shared_ptr<const void>; geometry::cpu_buffer::raw_data
    // is shared_ptr<void>. The contents are immutable in practice, but the types
    // differ — const_pointer_cast reuses the control block without a copy.
    cb.raw_data = std::const_pointer_cast<void>(cpu->data);
    cb.byte_size = cpu->byte_size;
    g.buffers.push_back(ossia::geometry::buffer{.data = cb, .dirty = true});
  }
  else if(auto* gpu = ossia::get_if<ossia::gpu_buffer_handle>(&br.resource))
  {
    ossia::geometry::gpu_buffer gb;
    gb.handle = gpu->native_handle;
    gb.byte_size = gpu->byte_size;
    g.buffers.push_back(ossia::geometry::buffer{.data = gb, .dirty = true});
  }
}

std::shared_ptr<ossia::geometry>
primitiveToGeometry(const ossia::mesh_primitive& prim)
{
  auto out = std::make_shared<ossia::geometry>();

  // 1) Buffers: one entry per vertex_buffer, optionally plus the index buffer.
  out->buffers.reserve(prim.vertex_buffers.size() + (prim.index_buffer ? 1 : 0));
  for(const auto& vb : prim.vertex_buffers)
  {
    if(vb)
      appendBufferResource(*out, *vb);
    else
      out->buffers.push_back(ossia::geometry::buffer{
          .data = ossia::geometry::gpu_buffer{}, .dirty = false});
  }
  const int index_buffer_idx = prim.index_buffer ? (int)out->buffers.size() : -1;
  if(prim.index_buffer)
    appendBufferResource(*out, *prim.index_buffer);

  // 2) Bindings: one per unique vertex_buffer_index. Stride taken from the
  //    first attribute landing in that binding.
  struct BindingInfo
  {
    uint32_t buffer_index{};
    uint32_t stride{};
    bool per_instance{};
  };
  std::vector<BindingInfo> bindings;
  auto findBinding = [&](uint32_t bi) -> int {
    for(std::size_t k = 0; k < bindings.size(); ++k)
      if(bindings[k].buffer_index == bi)
        return (int)k;
    return -1;
  };
  for(const auto& a : prim.attributes)
  {
    if(findBinding(a.buffer_index) < 0)
    {
      BindingInfo b;
      b.buffer_index = a.buffer_index;
      b.stride = a.byte_stride;
      b.per_instance
          = (a.rate == ossia::vertex_attribute::input_rate::per_instance);
      bindings.push_back(b);
    }
  }
  out->bindings.reserve(bindings.size());
  for(const auto& b : bindings)
  {
    ossia::geometry::binding gb{};
    gb.byte_stride = b.stride;
    gb.classification = b.per_instance
        ? ossia::geometry::binding::per_instance
        : ossia::geometry::binding::per_vertex;
    gb.step_rate = 1;
    out->bindings.push_back(gb);
  }

  // 3) Input: one entry per binding, pointing to the corresponding buffer.
  out->input.reserve(bindings.size());
  for(const auto& b : bindings)
  {
    // `input` resolves to an ossia-level type in this scope, so reference
    // the member type explicitly via a `struct` elaborated tag.
    struct ossia::geometry::input entry{};
    entry.buffer = (int)b.buffer_index;
    entry.byte_offset = 0;
    out->input.push_back(entry);
  }

  // 4) Attributes: remap buffer_index → binding index.
  out->attributes.reserve(prim.attributes.size());
  for(const auto& a : prim.attributes)
  {
    ossia::geometry::attribute ga{};
    ga.binding = findBinding(a.buffer_index);
    ga.location = 0; // resolved by the renderer's semantic remap
    ga.format = toGeomAttrFormat(a.format);
    ga.byte_offset = a.byte_offset;
    ga.semantic = a.semantic;
    out->attributes.push_back(ga);
  }

  // 5) Counts and topology.
  out->vertices = (int)prim.vertex_count;
  out->indices = (int)prim.index_count;
  out->instances = 1;
  out->topology = toGeomTopology(prim.topology);
  out->cull_mode = ossia::geometry::none;
  out->front_face = ossia::geometry::counter_clockwise;

  // 6) Index buffer reference.
  if(index_buffer_idx >= 0)
  {
    out->index.buffer = index_buffer_idx;
    out->index.byte_offset = 0;
    out->index.format = (prim.index_type == ossia::index_format::uint16)
        ? decltype(out->index)::uint16
        : decltype(out->index)::uint32;
  }
  else
  {
    out->index.buffer = -1;
  }

  // 7) Bounds.
  std::memcpy(out->bounds.min, prim.bounds.min, sizeof(float) * 3);
  std::memcpy(out->bounds.max, prim.bounds.max, sizeof(float) * 3);

  return out;
}

// Pack the CPU-side material_component into the 64-byte GPU-layout struct.
// Only factor fields are packed here; `textureRefs[]` are deliberately left
// at their default tex_ref_none() sentinel. ScenePreprocessorNode runs
// `rebuildChannel(ch)` for each of the four channels (BaseColor /
// MetalRough / Normal / Emissive) after the scene walk, which in turn
// calls `patchMaterialRefsFromCache(ch, fs)` (ScenePreprocessorNode.cpp:1944)
// to fill `fs.materials[i].textureRefs[ch]` with the assigned texture-array
// layer index per material per channel. Consumer shaders sample the
// per-channel arrays via `mat.textureRefs.x / .y / .z / .w` against
// `baseColorArray` / `metalRoughArray` / `normalArray` / `emissiveArray`.
MaterialGPU packMaterial(const ossia::material_component& mc)
{
  MaterialGPU gpu;
  std::memcpy(gpu.baseColor, mc.base_color_factor, sizeof(float) * 4);
  gpu.metallicRoughnessOcclusionUnlit[0] = mc.metallic_factor;
  gpu.metallicRoughnessOcclusionUnlit[1] = mc.roughness_factor;
  gpu.metallicRoughnessOcclusionUnlit[2] = mc.occlusion_strength;
  gpu.metallicRoughnessOcclusionUnlit[3] = mc.unlit ? 1.f : 0.f;
  gpu.emissive_strength[0] = mc.emissive_factor[0];
  gpu.emissive_strength[1] = mc.emissive_factor[1];
  gpu.emissive_strength[2] = mc.emissive_factor[2];
  gpu.emissive_strength[3] = mc.emissive_strength;

  // Feature mask — OR in a bit for each active BRDF lobe / texture.
  // Producers can override this at authoring time; when writing from
  // a scene_state.materials entry we derive from the CPU-side fields.
  // Used as SER reorder key + shader-side specialization branch.
  uint32_t fm = 0;
  using namespace material_feature;
  if(mc.base_color_texture.valid())         fm |= has_base_color_texture;
  if(mc.metallic_roughness_texture.valid()) fm |= has_metal_rough_texture;
  if(mc.normal_texture.valid())             fm |= has_normal_texture;
  if(mc.emissive_texture.valid())           fm |= has_emissive_texture;
  if(mc.unlit)                              fm |= unlit;
  if(mc.alpha != ossia::alpha_mode::opaque_) fm |= alpha_non_opaque;
  if(mc.alpha == ossia::alpha_mode::mask)   fm |= alpha_mask;
  if(mc.alpha == ossia::alpha_mode::blend)  fm |= alpha_blend;
  if(mc.double_sided)                       fm |= double_sided;
  // Scene-filter opt-outs — "disabled" semantics keep the common case
  // (caster = true) at 0. CSF filter shaders test these bits.
  if(!mc.shadow_caster)                     fm |= shadow_caster_disabled;
  if(!mc.reflection_caster)                 fm |= reflection_caster_disabled;
  // Occlusion: set the flag whenever the material has an occlusionTexture
  // at all — the shader samples through `mat.occlusion_textureRef`
  // unconditionally in the "separate" branch, which works for both
  // distinct-source and shared-with-MR (ORM) packings. Routing through
  // mr.r as a fallback when no occlusion_texture is present is unsafe:
  // the glTF spec leaves pbrMetallicRoughness.R undefined and most
  // authoring tools leave it at 0, which silently zeroes the ambient
  // floor / IBL occlusion multiplier and turns dark metals pitch-black.
  if(mc.occlusion_texture.valid())
    fm |= has_separate_occlusion;

  // Per-channel texcoord_set bits (20-29). Clamp to 1 — glTF allows
  // up to TEXCOORD_7 but our MDI layout carries TEXCOORD_0/1 only.
  auto pack_tcset = [](uint32_t set_idx, uint32_t shift) -> uint32_t {
    return (set_idx > 1u ? 1u : set_idx) << shift;
  };
  fm |= pack_tcset(mc.base_color_texture.texcoord_set,         20);
  fm |= pack_tcset(mc.metallic_roughness_texture.texcoord_set, 22);
  fm |= pack_tcset(mc.normal_texture.texcoord_set,             24);
  fm |= pack_tcset(mc.emissive_texture.texcoord_set,           26);
  fm |= pack_tcset(mc.occlusion_texture.texcoord_set,          28);
  if(mc.clearcoat.factor > 0.f)             fm |= has_clearcoat;
  if(mc.sheen.color_factor[0] > 0.f
     || mc.sheen.color_factor[1] > 0.f
     || mc.sheen.color_factor[2] > 0.f)     fm |= has_sheen;
  if(mc.transmission.factor > 0.f)          fm |= has_transmission;
  if(mc.volume.thickness_factor > 0.f)      fm |= has_volume;
  if(mc.specular.factor != 1.f
     || mc.specular.color_factor[0] != 1.f
     || mc.specular.color_factor[1] != 1.f
     || mc.specular.color_factor[2] != 1.f) fm |= has_specular;
  if(mc.iridescence.factor > 0.f)           fm |= has_iridescence;
  if(mc.anisotropy.strength != 0.f)         fm |= has_anisotropy;
  if(mc.diffuse_transmission.factor > 0.f)  fm |= has_diffuse_transmission;
  // Subsurface: OpenPBR; no equivalent in ossia material today.
  // thin_walled: OpenPBR; not in ossia today either.
  gpu.feature_mask = fm;

  // hit_group_id stays at default (0 = standard lit). A future
  // pipeline-build step can map feature_mask to a dedicated hit-group
  // index when RT lands; producers with a pre-computed mapping can
  // set this directly.
  gpu.hit_group_id = 0u;

  // alpha_cutoff: glTF spec default is 0.5; only consulted by the
  // shader when feature_mask carries `alpha_mask`.
  gpu.alpha_cutoff = mc.alpha_cutoff;

  // occlusion_textureRef stays at tex_ref_none() here — the texture
  // ref needs the resolved (bucket, layer) from
  // patchMaterialRefsFromCache. ScenePreprocessor patches it in the
  // 5th-channel pass.

  return gpu;
}

// Pack the OpenPBR / KHR extension fields from `material_component` into
// MaterialExtensionsGPU (272 B). Field order matches the struct's
// declaration — if you reorder there, reorder here.
//
// `textureRefs[]` is left at the default tex_ref_none() sentinels here.
// The encoded refs are written by ScenePreprocessor::patchMaterialRefs
// FromCache in lockstep with the base-channel refs: the
// `kExtTextureSlots` table in ScenePreprocessorNode.cpp routes each
// MaterialExtensionsGPU::textureRefs[slot] through one of the existing
// 5 channel pools (BaseColor / MetalRough / Normal) based on format
// expectation. No separate ext-channel pool / sampler set — the same
// bucket samplers serve both the main 5 channels and every glTF
// KHR_materials_* extension texture.
MaterialExtensionsGPU packMaterialExtensions(const ossia::material_component& mc)
{
  MaterialExtensionsGPU gpu{};  // default-init = OpenPBR spec defaults

  // Coat — maps to KHR_materials_clearcoat; coat_darkening is an
  // OpenPBR extension not in glTF today (defaults to 0 → no darkening).
  gpu.coat[0] = mc.clearcoat.factor;
  gpu.coat[1] = mc.clearcoat.roughness_factor;
  gpu.coat[2] = 1.5f;      // coat_ior default (glTF doesn't expose a per-coat IOR)
  gpu.coat[3] = 0.f;       // coat_darkening
  // Base-layer IOR — glTF's KHR_materials_ior applies here.
  // No OpenPBR field for base IOR directly; we use it in the specular lobe.

  // Fuzz / sheen
  gpu.fuzz_color[0] = mc.sheen.color_factor[0];
  gpu.fuzz_color[1] = mc.sheen.color_factor[1];
  gpu.fuzz_color[2] = mc.sheen.color_factor[2];
  gpu.fuzz_color[3] = mc.sheen.roughness_factor;

  // Transmission + volume. glTF separates thin-walled (transmission) from
  // volumetric (volume); OpenPBR folds them: transmission_weight is the
  // scalar knob, transmission_depth makes it volumetric. An infinite
  // attenuation_distance effectively means "no absorption" → depth = 0.
  gpu.transmission[0] = mc.transmission.factor;
  gpu.transmission[1] = std::isfinite(mc.volume.attenuation_distance)
                            ? mc.volume.attenuation_distance : 0.f;
  gpu.transmission[2] = 0.f;    // dispersion_scale — not in glTF
  gpu.transmission[3] = 20.f;   // dispersion Abbe number — crown-glass default
  gpu.transmission_color[0] = mc.volume.attenuation_color[0];
  gpu.transmission_color[1] = mc.volume.attenuation_color[1];
  gpu.transmission_color[2] = mc.volume.attenuation_color[2];
  gpu.transmission_color[3] = 0.f;    // scatter_anisotropy — not in glTF
  // transmission_scatter stays at zero (no volumetric scattering in glTF).

  // Specular (KHR_materials_specular)
  gpu.specular_weight_color[0] = mc.specular.factor;
  gpu.specular_weight_color[1] = mc.specular.color_factor[0];
  gpu.specular_weight_color[2] = mc.specular.color_factor[1];
  gpu.specular_weight_color[3] = mc.specular.color_factor[2];
  gpu.specular_ior_anisotropy[0] = mc.ior;
  gpu.specular_ior_anisotropy[1] = mc.anisotropy.strength;
  // Anisotropy rotation comes from material_component as a scalar angle
  // in radians; OpenPBR wants it split into cos/sin to skip per-fragment
  // trig. Bake it here.
  gpu.specular_ior_anisotropy[2] = std::cos(mc.anisotropy.rotation);
  gpu.specular_ior_anisotropy[3] = std::sin(mc.anisotropy.rotation);

  // Thin-film iridescence. glTF carries min/max thickness; OpenPBR
  // reference impl uses a single thickness (the film is nominally
  // uniform; spatial variation would need a texture). Average the two.
  gpu.thin_film[0] = mc.iridescence.factor;
  gpu.thin_film[1]
      = (mc.iridescence.thickness_min + mc.iridescence.thickness_max) * 0.5f;
  gpu.thin_film[2] = mc.iridescence.ior;

  // Diffuse transmission (KHR_materials_diffuse_transmission)
  gpu.diffuse_transmission[0] = mc.diffuse_transmission.factor;
  gpu.diffuse_transmission[1] = mc.diffuse_transmission.color_factor[0];
  gpu.diffuse_transmission[2] = mc.diffuse_transmission.color_factor[1];
  gpu.diffuse_transmission[3] = mc.diffuse_transmission.color_factor[2];

  // Subsurface — stock glTF has no SSS. FbxParser maps FBX
  // subsurface_factor / subsurface_color into
  // mc.diffuse_transmission as the nearest equivalent slot
  // (see FbxParser.cpp's KHR-extension mapping). We leave
  // subsurface_* at OpenPBR spec defaults (weight = 0) for the pure-
  // glTF case; when a loader grows a dedicated subsurface channel on
  // material_component we'll fill it here.

  // Flags: base diffuse roughness + thin-walled.
  // `thin_walled` lives in scene_property_map["thin_walled"] when
  // FbxParser sees an Arnold thin-walled feature. Presence of the key
  // alone means true — the loader inserts the entry only when the flag
  // is enabled. Application-level properties outside this hardcoded
  // list aren't consumed here.
  if(mc.properties.find("thin_walled") != mc.properties.end())
    gpu.flags[1] = 1.f;

  return gpu;
}

// Visitor that walks the scene_payload tree and collects draw calls, lights, cameras.
struct FlattenVisitor
{
  FlatScene& out;
  QMatrix4x4 parentWorld;
  ossia::scene_node_id currentNodeId{};
  // KHR_materials_variants: set from scene_state::active_variant_index
  // at flatten-start. -1 = use each primitive's default material.
  int32_t activeVariant{-1};

  // Most recently encountered producer-authored scene_transform slot on
  // the current walk path. 0xFFFFFFFF = none yet. Stamped on each
  // DrawCall so PerDrawGPU.transform_slot can point at the corresponding
  // world_transforms / world_transforms_prev entry for motion vectors.
  std::uint32_t currentTransformSlot{0xFFFFFFFFu};

  void visitPayload(const ossia::scene_payload& payload)
  {
    if(auto* subnode = ossia::get_if<ossia::scene_node_ptr>(&payload))
    {
      if(*subnode)
        visitNode(**subnode);
    }
    else if(auto* mesh = ossia::get_if<ossia::mesh_component_ptr>(&payload))
    {
      if(*mesh)
        visitMesh(**mesh);
    }
    else if(auto* light = ossia::get_if<ossia::light_component_ptr>(&payload))
    {
      if(*light)
      {
        // Arena slot index for shader-side arena-direct light reads
        // (task 28b/c — packLight path removed). 0xFFFFFFFF sentinel
        // for producer-less lights (e.g. FBX/glTF-embedded lights that
        // don't own a RawLight slot yet). Such lights are filtered out
        // when building scene_light_indices.
        out.lightArenaSlots.push_back(
            (*light)->raw_slot.size != 0
                ? (*light)->raw_slot.internal_index
                : 0xFFFFFFFFu);
      }
    }
    else if(auto* camera = ossia::get_if<ossia::camera_component_ptr>(&payload))
    {
      if(*camera)
      {
        FlatScene::CameraEntry e;
        e.component = *camera;
        e.worldTransform = parentWorld;
        e.node_id = currentNodeId;
        out.cameras.push_back(std::move(e));
      }
    }
    else if(auto* xform = ossia::get_if<ossia::scene_transform>(&payload))
    {
      // A bare transform applies to subsequent siblings — update parentWorld
      parentWorld = parentWorld * toQMatrix(*xform);
      // Emit the composed world matrix in walk order so the preprocessor
      // can upload it into its private world-transforms SSBO. Only
      // producer-authored transforms (stamped raw_slot) get an entry —
      // loader-interior transforms participate in hierarchy accumulation
      // but aren't individually addressable on GPU.
      if(xform->raw_slot.size != 0)
      {
        out.worldTransforms.push_back(
            WorldTransformEmit{parentWorld, xform->raw_slot.internal_index});
        // Remember this slot as the "nearest producer transform" so
        // subsequent sibling / child draws can reference it for
        // motion-vector / TAA lookups via world_transforms_prev[slot].
        currentTransformSlot = xform->raw_slot.internal_index;
      }
    }
    else if(auto* sd = ossia::get_if<ossia::scene_data_ptr>(&payload))
    {
      // Generic escape hatch: stash it; the ScenePreprocessor forwards every entry
      // as an auxiliary_buffer on the output geometry.
      if(*sd)
        out.scene_data.push_back(*sd);
    }
    else if(auto* inst = ossia::get_if<ossia::instance_component_ptr>(&payload))
    {
      // GPU-instanced mesh: collect — the ScenePreprocessor emits one DrawCall with
      // instances=instance_count and forwards the instance SSBOs.
      if(*inst)
        out.instances.push_back({*inst, parentWorld});
    }
    // gaussian_splat, voxel_field, point_cloud, volume — not rendered yet,
    // but the types are transported. Renderers will handle them later.
  }

  void visitNode(const ossia::scene_node& node)
  {
    // Inactive nodes are skipped entirely — no transforms, no children,
    // no payload contributions. USD-style non-destructive prune: the
    // data stays in the scene tree so downstream toggles can
    // re-activate without re-uploading geometry.
    if(!node.active)
      return;

    // scene_node has no transform of its own in the new design.
    // Transforms are scene_payload children (scene_transform).
    // We process children in order; transform payloads affect subsequent siblings.
    if(!node.has_children())
      return;

    // Save current world so sibling transforms don't leak. Also remember the
    // parent node id so camera payloads can be attributed to it for
    // active_camera_id resolution. currentTransformSlot is save/restored
    // alongside parentWorld — a scene_transform encountered inside this
    // node's children scope shouldn't leak to unrelated siblings.
    QMatrix4x4 savedWorld = parentWorld;
    auto savedNodeId = currentNodeId;
    auto savedTransformSlot = currentTransformSlot;
    currentNodeId = node.id;

    for(auto& child : *node.children)
    {
      visitPayload(child);
    }

    parentWorld = savedWorld;
    currentNodeId = savedNodeId;
    currentTransformSlot = savedTransformSlot;
  }

  void visitMesh(const ossia::mesh_component& mc)
  {
    // Modern path: mesh_primitive[]. Build a transient ossia::geometry per
    // primitive so the ScenePreprocessor can treat it uniformly with legacy geometry.
    for(const auto& prim : mc.primitives)
    {
      if(prim.vertex_buffers.empty() || prim.vertex_count == 0)
        continue;
      DrawCall dc;
      dc.owned_mesh = primitiveToGeometry(prim);
      dc.mesh = dc.owned_mesh.get();
      // Prefer the producer-stamped stable_id (identity survives merge
      // reshuffles AND source-primitive pointer churn on rebuilds).
      // Fall back to the pointer bits when the producer hasn't stamped
      // one yet — legacy behaviour.
      dc.stable_id
          = prim.stable_id != 0
                ? prim.stable_id
                : reinterpret_cast<uint64_t>(&prim);
      dc.worldTransform = parentWorld;
      // Direct pointers — identity survives merge_scenes without a bias
      // table. flattenScene dedups these into FlatScene::materials /
      // ::skins after the walk and stamps the corresponding indices.
      dc.material = prim.material;
      // KHR_materials_variants override: when the active variant has
      // a non-null mapping for this primitive, swap in the variant's
      // material. Out-of-range / null entries fall through to default.
      if(activeVariant >= 0
         && (std::size_t)activeVariant < prim.material_variants.size()
         && prim.material_variants[activeVariant])
      {
        dc.material = prim.material_variants[activeVariant];
      }
      dc.skin = mc.skin;
      dc.local_bounds = prim.bounds;
      dc.transform_slot = currentTransformSlot;
      out.draws.push_back(std::move(dc));
    }

    // Legacy geometry_spec path (backward compat for loaders that still use
    // mesh_component::legacy_geometry).
    auto& geom_spec = mc.legacy_geometry;
    if(geom_spec.meshes && !geom_spec.meshes->meshes.empty())
    {
      for(auto& geom : geom_spec.meshes->meshes)
      {
        DrawCall dc;
        dc.mesh = &geom;
        // Legacy geometry has no producer-stamped stable_id field;
        // fall back to its address.
        dc.stable_id = reinterpret_cast<uint64_t>(&geom);
        dc.geometry_ref = geom_spec;
        dc.worldTransform = parentWorld;
        // Material comes from the first primitive if any, else null.
        if(!mc.primitives.empty())
          dc.material = mc.primitives[0].material;
        dc.skin = mc.skin;
        // Legacy path: fall back to mesh_component bounds (primitive
        // bounds may be absent on the old path). The preprocessor
        // treats empty bounds as "never cull".
        dc.local_bounds = mc.bounds;
        dc.transform_slot = currentTransformSlot;
        out.draws.push_back(std::move(dc));
      }
    }
  }

};

void flattenScene(const ossia::scene_spec& scene, FlatScene& out, float aspectRatio)
{
  out.clear();

  if(!scene.state || scene.state->empty())
    return;

  // Pack materials — base + extensions in lockstep. Both vectors grow
  // together so `material_extensions[i]` always corresponds to
  // `materials[i]`. Missing extension data (no KHR_* extension on a
  // given glTF material) lands as the default-constructed struct,
  // which is the OpenPBR spec default (all lobe weights = 0, IORs at
  // 1.5, etc.) — consumer shaders can blindly read it and get
  // identity behaviour where the file didn't opt in.
  if(scene.state->materials)
  {
    for(auto& mat : *scene.state->materials)
    {
      if(mat)
      {
        out.materials.push_back(packMaterial(*mat));
        out.material_extensions.push_back(packMaterialExtensions(*mat));
      }
      else
      {
        out.materials.push_back(MaterialGPU{});
        out.material_extensions.push_back(MaterialExtensionsGPU{});
      }
    }
  }

  // Pack skeletons: forward kinematics through joint hierarchy, then
  // joint_matrix[i] = world_joint[i] × inverse_bind_matrix[i]. Matches the
  // glTF skinning convention; consumer shaders multiply vertex position by
  // Σ(w_j × joint_matrix[j]).
  if(scene.state->skeletons)
  {
    auto jointLocal = [](const ossia::skeleton_joint& j) {
      QMatrix4x4 m;
      m.translate(j.translation[0], j.translation[1], j.translation[2]);
      m.rotate(QQuaternion(j.rotation[3], j.rotation[0], j.rotation[1], j.rotation[2]));
      m.scale(j.scale[0], j.scale[1], j.scale[2]);
      return m;
    };

    out.skins.reserve(scene.state->skeletons->size());
    for(const auto& sk : *scene.state->skeletons)
    {
      SkeletonGPU sg;
      if(!sk)
      {
        out.skins.push_back(std::move(sg));
        continue;
      }

      // Joints are expected to be topologically ordered: parent_index < i.
      // If a loader ever emits out-of-order joints, a future version can
      // do a multi-pass resolve.
      std::vector<QMatrix4x4> world(sk->joints.size());
      sg.joint_matrices.resize(sk->joints.size());
      for(std::size_t i = 0; i < sk->joints.size(); ++i)
      {
        const auto& j = sk->joints[i];
        QMatrix4x4 local = jointLocal(j);
        if(j.parent_index >= 0 && j.parent_index < (int32_t)i)
          world[i] = world[j.parent_index] * local;
        else
          world[i] = local;

        const QMatrix4x4 ibm = QMatrix4x4(j.inverse_bind_matrix, 4, 4);
        sg.joint_matrices[i] = world[i] * ibm;
      }
      out.skins.push_back(std::move(sg));
    }
  }

  // Walk the node tree. mesh_primitive / mesh_component now carry
  // direct shared_ptr references to their material and skin, so no
  // per-root index-bias bookkeeping is required.
  QMatrix4x4 identity;
  FlattenVisitor vis{out, identity};
  // KHR_materials_variants: seed the visitor from scene_state. When
  // no variants are declared (typical) this stays at -1 and the
  // per-draw override branch compiles to a cheap null-check.
  vis.activeVariant = scene.state->active_variant_index;
  const auto& roots = *scene.state->roots;
  for(std::size_t ri = 0; ri < roots.size(); ++ri)
  {
    if(!roots[ri])
      continue;
    vis.visitNode(*roots[ri]);
  }

  // Resolve DrawCall::materialIndex / ::skinIndex from the direct
  // shared_ptr references stamped on each draw. materialIndex is the
  // position of dc.material inside scene.state->materials (packed
  // above into out.materials in the same order), so the shaders can
  // continue to SSBO-index into scene_materials[draw.material_index].
  if(scene.state->materials && !scene.state->materials->empty())
  {
    ossia::hash_map<const ossia::material_component*, int> mat_index;
    mat_index.reserve(scene.state->materials->size());
    for(std::size_t i = 0; i < scene.state->materials->size(); ++i)
    {
      const auto& m = (*scene.state->materials)[i];
      if(m)
        mat_index[m.get()] = (int)i;
    }
    for(auto& dc : out.draws)
    {
      if(!dc.material)
        continue;
      auto it = mat_index.find(dc.material.get());
      dc.materialIndex = (it != mat_index.end()) ? it->second : -1;
    }
  }
  if(scene.state->skeletons && !scene.state->skeletons->empty())
  {
    ossia::hash_map<const ossia::skeleton_component*, int> skin_index;
    skin_index.reserve(scene.state->skeletons->size());
    for(std::size_t i = 0; i < scene.state->skeletons->size(); ++i)
    {
      const auto& s = (*scene.state->skeletons)[i];
      if(s)
        skin_index[s.get()] = (int)i;
    }
    for(auto& dc : out.draws)
    {
      if(!dc.skin)
        continue;
      auto it = skin_index.find(dc.skin.get());
      dc.skinIndex = (it != skin_index.end()) ? it->second : -1;
    }
  }

  // Also surface any cameras registered at scene_state level (producers
  // that don't want to embed a camera node can publish via `cameras` only).
  if(scene.state->cameras)
  {
    for(const auto& cam : *scene.state->cameras)
    {
      if(!cam)
        continue;
      FlatScene::CameraEntry e;
      e.component = cam;
      // No world transform context at this level — identity placement.
      e.worldTransform = QMatrix4x4{};
      out.cameras.push_back(std::move(e));
    }
  }

  // Resolve active camera: match scene_state.active_camera_id against the
  // collected camera entries; fall back to the first camera if the id is
  // unset or not found.
  if(!out.cameras.empty())
  {
    out.activeCameraIndex = 0;
    if(scene.state->active_camera_id.value != 0)
    {
      for(std::size_t i = 0; i < out.cameras.size(); ++i)
      {
        if(out.cameras[i].node_id == scene.state->active_camera_id)
        {
          out.activeCameraIndex = (int)i;
          break;
        }
      }
    }
  }

  // Populate legacy single-camera mirror fields so consumers that haven't
  // migrated to `cameras[activeCameraIndex]` keep working.
  if(out.activeCameraIndex >= 0)
  {
    const auto& e = out.cameras[(std::size_t)out.activeCameraIndex];
    const auto& cam = *e.component;
    out.cameraPosition = e.worldTransform.column(3).toVector3D();
    out.viewMatrix = e.worldTransform.inverted();
    out.cameraFov = cam.yfov * (180.f / float(M_PI));
    out.cameraNear = cam.znear;
    out.cameraFar = cam.zfar;
    out.projectionMatrix.setToIdentity();
    out.projectionMatrix.perspective(
        out.cameraFov, aspectRatio, out.cameraNear, out.cameraFar);
    out.hasCamera = true;
  }
  else
  {
    out.cameraPosition = QVector3D(0.f, 0.f, 3.f);
    out.viewMatrix.setToIdentity();
    out.viewMatrix.lookAt(
        out.cameraPosition, QVector3D(0.f, 0.f, 0.f), QVector3D(0.f, 1.f, 0.f));
    out.projectionMatrix.setToIdentity();
    out.projectionMatrix.perspective(60.f, aspectRatio, 0.1f, 1000.f);
    out.cameraFov = 60.f;
    out.cameraNear = 0.1f;
    out.cameraFar = 1000.f;
    out.hasCamera = false;
  }
}

}
