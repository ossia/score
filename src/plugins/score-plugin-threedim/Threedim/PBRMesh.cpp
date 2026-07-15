#include "PBRMesh.hpp"

#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/SceneGPUState.hpp>

#include <QMatrix4x4>
#include <QQuaternion>

#include <algorithm>
#include <cstring>

namespace Threedim
{

namespace
{

// halp::attribute_format → ossia::vertex_format. The enum orderings differ,
// so this has to be a switch rather than a static_cast. Unknown formats
// fall back to float3 (the most common vertex-attribute case).
ossia::vertex_format mapFormat(halp::attribute_format f) noexcept
{
  using H = halp::attribute_format;
  using O = ossia::vertex_format;
  switch(f)
  {
    case H::float1:     return O::float1;
    case H::float2:     return O::float2;
    case H::float3:     return O::float3;
    case H::float4:     return O::float4;
    case H::half1:      return O::half1;
    case H::half2:      return O::half2;
    case H::half3:      return O::half3;
    case H::half4:      return O::half4;
    case H::unormbyte1: return O::unorm8x1;
    case H::unormbyte2: return O::unorm8x2;
    case H::unormbyte4: return O::unorm8x4;
    case H::uint1:      return O::uint32x1;
    case H::uint2:      return O::uint32x2;
    case H::uint3:      return O::uint32x3;
    case H::uint4:      return O::uint32x4;
    case H::sint1:      return O::sint32x1;
    case H::sint2:      return O::sint32x2;
    case H::sint3:      return O::sint32x3;
    case H::sint4:      return O::sint32x4;
    case H::ushort1:    return O::uint16x1;
    case H::ushort2:    return O::uint16x2;
    case H::ushort4:    return O::uint16x4;
    case H::sshort1:    return O::sint16x1;
    case H::sshort2:    return O::sint16x2;
    case H::sshort4:    return O::sint16x4;
    default:            return O::float3;
  }
}

ossia::primitive_topology mapTopology(halp::primitive_topology t) noexcept
{
  using H = halp::primitive_topology;
  using O = ossia::primitive_topology;
  switch(t)
  {
    case H::triangles:      return O::triangles;
    case H::triangle_strip: return O::triangle_strip;
    case H::triangle_fan:   return O::triangle_fan;
    case H::lines:          return O::lines;
    case H::line_strip:     return O::line_strip;
    case H::points:         return O::points;
  }
  return O::triangles;
}

ossia::index_format mapIndexFormat(halp::index_format f) noexcept
{
  return (f == halp::index_format::uint16) ? ossia::index_format::uint16
                                           : ossia::index_format::uint32;
}

// Wrap a halp GPU buffer handle into an ossia::buffer_resource carrying a
// gpu_buffer_handle (no CPU-side data, no upload). Returns null on a null
// handle so caller can skip that slot.
ossia::buffer_resource_ptr
wrapGpuBuffer(void* handle, int64_t byte_size) noexcept
{
  if(!handle)
    return nullptr;
  ossia::gpu_buffer_handle gh;
  gh.native_handle = handle;
  gh.byte_size = byte_size;
  gh.byte_offset = 0;
  auto res = std::make_shared<ossia::buffer_resource>();
  res->resource = gh;
  res->dirty_index = 1;
  return res;
}

} // namespace

void PBRMesh::operator()()
{
  if(m_material_stable_id == 0) m_material_stable_id = ossia::mint_stable_id();
  if(m_primitive_stable_id == 0) m_primitive_stable_id = ossia::mint_stable_id();
  if(m_xform_stable_id == 0) m_xform_stable_id = ossia::mint_stable_id();

  const auto& m = inputs.geometry_in.mesh;
  void* buf0_handle
      = m.buffers.empty() ? nullptr : m.buffers[0].handle;

  // Identity-caching fast path: skip the rebuild when the input
  // geometry buffers / counts / textures / factors are all unchanged.
  const float cur_factors[10]{
      inputs.base_r.value, inputs.base_g.value, inputs.base_b.value,
      inputs.base_a.value, inputs.metallic.value, inputs.roughness.value,
      inputs.em_r.value, inputs.em_g.value, inputs.em_b.value,
      inputs.em_strength.value};
  void* cur_tex[4]{
      inputs.base_color_tex.texture.handle,
      inputs.metal_rough_tex.texture.handle,
      inputs.normal_tex.texture.handle,
      inputs.emissive_tex.texture.handle};

  float scratch[16];
  CachedTRS xformCache = m_cachedTRS;
  const bool trs_changed = computeTRSMatrix(inputs, scratch, xformCache);

  // Intentionally NOT gating on `inputs.geometry_in.dirty_mesh`: upstream
  // CSF compute nodes raise that flag every frame to signal content
  // changed, but our downstream ScenePreprocessor handles content changes
  // via its GPU-copy path (which re-fires every runInitialPasses). Only
  // STRUCTURAL changes — buffer-handle swap, vertex/index count change,
  // texture-override swap, factor change — need a new scene_state
  // version; content-only changes keep the cached shared_ptr so
  // ScenePreprocessor's fingerprint fast-path stays warm and doesn't
  // rebuild the MDI merge + invalidate downstream pipeline state.
  const bool inputs_changed
      = m_cached_buf0 != buf0_handle
        || m_cached_vertices != m.vertices
        || m_cached_indices != m.indices
        || !std::equal(m_cached_tex, m_cached_tex + 4, cur_tex)
        || !std::equal(m_cached_factors, m_cached_factors + 10, cur_factors);

  if(!inputs_changed && !trs_changed && m_wrapped_state && buf0_handle)
  {
    outputs.scene_out.scene.state = m_wrapped_state;
    outputs.scene_out.dirty = 0;
    return;
  }
  m_cachedTRS = xformCache;
  m_cached_buf0 = buf0_handle;
  m_cached_vertices = m.vertices;
  m_cached_indices = m.indices;
  std::copy(cur_tex, cur_tex + 4, m_cached_tex);
  std::copy(cur_factors, cur_factors + 10, m_cached_factors);

  if(!buf0_handle || m.vertices <= 0)
  {
    outputs.scene_out.scene = {};
    m_wrapped_state.reset();
    return;
  }

  // Wrap halp buffers → ossia buffer_resources (parallel indexing so
  // attribute buffer_index resolution is a direct lookup).
  ossia::small_vector<ossia::buffer_resource_ptr, 4> wrapped_buffers;
  wrapped_buffers.reserve(m.buffers.size());
  for(const auto& b : m.buffers)
    wrapped_buffers.push_back(wrapGpuBuffer(b.handle, b.byte_size));

  // Build one mesh_primitive off the geometry.
  ossia::mesh_primitive mp;
  // vertex_buffers parallel to halp's buffers so attr.buffer_index resolves
  // directly. Leaves nulls in place — attributes whose buffer is null are
  // filtered out on the attribute walk below.
  for(const auto& w : wrapped_buffers)
    if(w)
      mp.vertex_buffers.push_back(w);

  // Map halp buffer index → mp.vertex_buffers index (we may have dropped
  // nulls along the way).
  ossia::small_vector<int, 4> bufRemap;
  bufRemap.resize(wrapped_buffers.size(), -1);
  int out_idx = 0;
  for(std::size_t i = 0; i < wrapped_buffers.size(); ++i)
  {
    if(wrapped_buffers[i])
      bufRemap[i] = out_idx++;
  }

  for(const auto& attr : m.attributes)
  {
    if(attr.binding < 0 || attr.binding >= (int)m.input.size())
      continue;
    const auto& in = m.input[attr.binding];
    if(in.buffer < 0 || in.buffer >= (int)bufRemap.size())
      continue;
    const int buf_idx = bufRemap[in.buffer];
    if(buf_idx < 0)
      continue;

    ossia::vertex_attribute va;
    va.semantic = static_cast<ossia::attribute_semantic>(attr.semantic);
    va.format = mapFormat(attr.format);
    va.buffer_index = (uint32_t)buf_idx;
    va.byte_offset = uint32_t(in.byte_offset + attr.byte_offset);
    // Binding stride governs per-vertex advance; fall back to 0 (tightly
    // packed single attribute) if the binding entry is missing.
    va.byte_stride = (attr.binding < (int)m.bindings.size())
                         ? (uint32_t)m.bindings[attr.binding].stride
                         : 0u;
    va.rate = ossia::vertex_attribute::input_rate::per_vertex;
    mp.attributes.push_back(va);
  }

  // Index buffer (optional).
  if(m.index.buffer >= 0 && m.index.buffer < (int)m.buffers.size())
  {
    const auto& ib = m.buffers[m.index.buffer];
    if(ib.handle)
    {
      ossia::gpu_buffer_handle gh;
      gh.native_handle = ib.handle;
      gh.byte_size = ib.byte_size;
      gh.byte_offset = m.index.byte_offset;
      auto ibr = std::make_shared<ossia::buffer_resource>();
      ibr->resource = gh;
      ibr->dirty_index = 1;
      mp.index_buffer = ibr;
      mp.index_type = mapIndexFormat(m.index.format);
    }
  }

  mp.topology = mapTopology(m.topology);
  mp.stable_id = m_primitive_stable_id;
  mp.vertex_count = (uint32_t)std::max(0, m.vertices);
  mp.index_count = (uint32_t)std::max(0, m.indices);

  // Author the material. Factors come from the controls; texture slots
  // populate the dynamic-handle pathway when the corresponding inlet
  // carries a non-null handle. The primitive's `material` is bound to
  // this shared_ptr directly — no index lookup.
  auto mat = std::make_shared<ossia::material_component>();
  mat->stable_id = m_material_stable_id;
  mat->base_color_factor[0] = cur_factors[0];
  mat->base_color_factor[1] = cur_factors[1];
  mat->base_color_factor[2] = cur_factors[2];
  mat->base_color_factor[3] = cur_factors[3];
  mat->metallic_factor = cur_factors[4];
  mat->roughness_factor = cur_factors[5];
  mat->emissive_factor[0] = cur_factors[6];
  mat->emissive_factor[1] = cur_factors[7];
  mat->emissive_factor[2] = cur_factors[8];
  mat->emissive_strength = cur_factors[9];

  auto stamp_tex = [](ossia::texture_ref& dst, void* h) {
    if(!h)
      return;
    dst.texture.native_handle = h;
    dst.texture.bindless_index = 0;
    dst.source.reset();
  };
  stamp_tex(mat->base_color_texture, cur_tex[0]);
  stamp_tex(mat->metallic_roughness_texture, cur_tex[1]);
  stamp_tex(mat->normal_texture, cur_tex[2]);
  stamp_tex(mat->emissive_texture, cur_tex[3]);

  // Propagate the Material arena slot ref (populated in init()).
  mat->raw_slot = m_material_ref;

  mp.material = ossia::material_component_ptr(mat);

  auto mesh_comp = std::make_shared<ossia::mesh_component>();
  mesh_comp->primitives.push_back(std::move(mp));

  // Assemble the single scene_node: TRS first (Loader convention), then
  // the mesh_component as the second payload. Matches GltfParser's
  // layout so the built-in TRS controls act on the mesh the same way.
  ossia::scene_transform xform;
  xform.stable_id = m_xform_stable_id;
  xform.translation[0] = inputs.position.value.x;
  xform.translation[1] = inputs.position.value.y;
  xform.translation[2] = inputs.position.value.z;
  auto q = QQuaternion::fromEulerAngles(
      inputs.rotation.value.x, inputs.rotation.value.y,
      inputs.rotation.value.z);
  xform.rotation[0] = q.x();
  xform.rotation[1] = q.y();
  xform.rotation[2] = q.z();
  xform.rotation[3] = q.scalar();
  xform.scale[0] = inputs.scale.value.x;
  xform.scale[1] = inputs.scale.value.y;
  xform.scale[2] = inputs.scale.value.z;
  // Propagate the RawTransform slot ref (populated in init()).
  xform.raw_slot = m_xform_ref;

  auto children = std::make_shared<std::vector<ossia::scene_payload>>();
  children->push_back(xform);
  children->push_back(ossia::mesh_component_ptr(std::move(mesh_comp)));

  auto node = std::make_shared<ossia::scene_node>();
  node->children = std::move(children);
  node->dirty_index = ++m_version_counter;

  auto roots = std::make_shared<std::vector<ossia::scene_node_ptr>>();
  roots->push_back(std::move(node));

  auto mats = std::make_shared<std::vector<ossia::material_component_ptr>>();
  mats->push_back(std::move(mat));

  auto state = std::make_shared<ossia::scene_state>();
  state->roots = std::move(roots);
  state->materials = std::move(mats);
  state->version = m_version_counter;
  state->dirty_index = m_version_counter;

  m_wrapped_state = std::move(state);
  outputs.scene_out.scene.state = m_wrapped_state;
  outputs.scene_out.dirty = 0xFF;
}

void PBRMesh::init(score::gfx::RenderList& r, QRhiResourceUpdateBatch& res)
{
  // One slot in the Material arena per PBRMesh for its lifetime. Seeded
  // with default-constructed MaterialGPU bytes so any reader that samples
  // the slot before the first update() sees a neutral white material
  // rather than undefined memory.
  if(!material_slot.valid())
  {
    material_slot = r.registry().allocate(
        score::gfx::GpuResourceRegistry::Arena::Material,
        sizeof(score::gfx::MaterialGPU));
    m_material_ref = r.registry().toOssiaRef(material_slot);
  }
  if(material_slot.valid())
  {
    score::gfx::MaterialGPU seed{};
    r.registry().updateSlot(res, material_slot, &seed, sizeof(seed));
  }
  if(!raw_transform_slot.valid())
  {
    raw_transform_slot = r.registry().allocate(
        score::gfx::GpuResourceRegistry::Arena::RawTransform,
        sizeof(score::gfx::RawLocalTransform));
    m_xform_ref = r.registry().toOssiaRef(raw_transform_slot);
  }
  if(raw_transform_slot.valid())
  {
    score::gfx::RawLocalTransform seed{};
    r.registry().updateSlot(res, raw_transform_slot, &seed, sizeof(seed));
  }
}

void PBRMesh::update(
    score::gfx::RenderList& r, QRhiResourceUpdateBatch& res,
    score::gfx::Edge*)
{
  // Pack control-snapshot factor values into the slot, and — when a
  // runtime GPU handle is wired on one of the four channels — register
  // a dynamic slot in the registry and stamp tex_ref_dynamic(slot) into
  // the slot's textureRefs[]. Producer-authored refs agree with the
  // preprocessor's own rebuildDynamicSlots because both go through the
  // same shared registry map — idempotent.
  if(!material_slot.valid())
    return;

  score::gfx::MaterialGPU gpu{};
  gpu.baseColor[0] = inputs.base_r.value;
  gpu.baseColor[1] = inputs.base_g.value;
  gpu.baseColor[2] = inputs.base_b.value;
  gpu.baseColor[3] = inputs.base_a.value;
  gpu.metallicRoughnessOcclusionUnlit[0] = inputs.metallic.value;
  gpu.metallicRoughnessOcclusionUnlit[1] = inputs.roughness.value;
  gpu.metallicRoughnessOcclusionUnlit[2] = 1.f;
  gpu.metallicRoughnessOcclusionUnlit[3] = 0.f;
  gpu.emissive_strength[0] = inputs.em_r.value;
  gpu.emissive_strength[1] = inputs.em_g.value;
  gpu.emissive_strength[2] = inputs.em_b.value;
  gpu.emissive_strength[3] = inputs.em_strength.value;

  using Ch = score::gfx::GpuResourceRegistry::TextureChannel;
  uint32_t fm = 0u;
  using namespace score::gfx::material_feature;
  auto stamp_dyn = [&](Ch ch, void* handle, int idx, uint32_t feature_bit) {
    if(!handle)
      return;
    const int slot = r.registry().resolveDynamicSlot(ch, handle);
    if(slot < 0)
      return;
    gpu.textureRefs[idx] = score::gfx::tex_ref_dynamic((uint32_t)slot);
    fm |= feature_bit;
  };
  stamp_dyn(Ch::BaseColor,  inputs.base_color_tex.texture.handle,   0, has_base_color_texture);
  stamp_dyn(Ch::MetalRough, inputs.metal_rough_tex.texture.handle,  1, has_metal_rough_texture);
  stamp_dyn(Ch::Normal,     inputs.normal_tex.texture.handle,       2, has_normal_texture);
  stamp_dyn(Ch::Emissive,   inputs.emissive_tex.texture.handle,     3, has_emissive_texture);

  // PBRMesh is lit PBR (unlit flag not exposed), fully opaque by default.
  // No extension lobes wired through the current control surface. As
  // extension support grows on this node we OR additional feature bits.
  gpu.feature_mask = fm;
  // hit_group_id stays 0 = standard lit; RT pipeline build will swap in
  // a mask-specific index when relevant.

  r.registry().updateSlot(res, material_slot, &gpu, sizeof(gpu));

  if(raw_transform_slot.valid())
  {
    score::gfx::RawLocalTransform xform{};
    xform.translation[0] = inputs.position.value.x;
    xform.translation[1] = inputs.position.value.y;
    xform.translation[2] = inputs.position.value.z;
    QQuaternion q = QQuaternion::fromEulerAngles(
        inputs.rotation.value.x, inputs.rotation.value.y,
        inputs.rotation.value.z);
    xform.rotation[0] = q.x();
    xform.rotation[1] = q.y();
    xform.rotation[2] = q.z();
    xform.rotation[3] = q.scalar();
    xform.scale[0] = inputs.scale.value.x;
    xform.scale[1] = inputs.scale.value.y;
    xform.scale[2] = inputs.scale.value.z;
    r.registry().updateSlot(res, raw_transform_slot, &xform, sizeof(xform));
  }
}

void PBRMesh::release(score::gfx::RenderList& r)
{
  if(material_slot.valid())
    r.registry().free(material_slot);
  if(raw_transform_slot.valid())
    r.registry().free(raw_transform_slot);
  m_material_ref = {};
  m_xform_ref = {};
  // Producer-state-drift Option A — see Light::release.
  m_wrapped_state.reset();
}

} // namespace Threedim
