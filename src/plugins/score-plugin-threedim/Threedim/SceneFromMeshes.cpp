#include "SceneFromMeshes.hpp"

#include <cstring>
#include <memory>
#include <utility>

namespace Threedim
{

namespace
{

// Map a Threedim::mesh::extras[].semantic (which is halp::attribute_semantic)
// onto the ossia::attribute_semantic enum. Both use the same naming
// convention for the common cases; fall back to `custom` for anything the
// halp enum encodes that ossia doesn't name explicitly. The extra's
// `.name` field is kept alongside custom attributes so downstream shaders
// can match by string.
static ossia::attribute_semantic
translateExtraSemantic(const Threedim::extra_attribute& e) noexcept
{
  using H = halp::attribute_semantic;
  switch(e.semantic)
  {
    case H::position:  return ossia::attribute_semantic::position;
    case H::normal:    return ossia::attribute_semantic::normal;
    case H::tangent:   return ossia::attribute_semantic::tangent;
    case H::bitangent: return ossia::attribute_semantic::bitangent;
    case H::texcoord0: return ossia::attribute_semantic::texcoord0;
    case H::texcoord1: return ossia::attribute_semantic::texcoord1;
    case H::color0:    return ossia::attribute_semantic::color0;
    case H::color1:    return ossia::attribute_semantic::color1;
    default:           return ossia::attribute_semantic::custom;
  }
}

// Translate halp::attribute_format → ossia::vertex_format. halp encodes
// both base type and component count in a single enum value (float1..4,
// sint1..4, uint1..4, etc). `components` is carried separately on the
// extra_attribute for shader layout but the conversion is format-driven.
// Unknown → float3 as a safe default.
static ossia::vertex_format
translateFormat(halp::attribute_format f, int /*components*/) noexcept
{
  using F = halp::attribute_format;
  switch(f)
  {
    case F::float1:     return ossia::vertex_format::float1;
    case F::float2:     return ossia::vertex_format::float2;
    case F::float3:     return ossia::vertex_format::float3;
    case F::float4:     return ossia::vertex_format::float4;
    case F::half1:      return ossia::vertex_format::half1;
    case F::half2:      return ossia::vertex_format::half2;
    case F::half3:      return ossia::vertex_format::half3;
    case F::half4:      return ossia::vertex_format::half4;
    case F::uint1:      return ossia::vertex_format::uint32x1;
    case F::uint2:      return ossia::vertex_format::uint32x2;
    case F::uint3:      return ossia::vertex_format::uint32x3;
    case F::uint4:      return ossia::vertex_format::uint32x4;
    case F::sint1:      return ossia::vertex_format::sint32x1;
    case F::sint2:      return ossia::vertex_format::sint32x2;
    case F::sint3:      return ossia::vertex_format::sint32x3;
    case F::sint4:      return ossia::vertex_format::sint32x4;
    case F::unormbyte1: return ossia::vertex_format::unorm8x1;
    case F::unormbyte2: return ossia::vertex_format::unorm8x2;
    case F::unormbyte4: return ossia::vertex_format::unorm8x4;
    case F::ushort1:    return ossia::vertex_format::uint16x1;
    case F::ushort2:    return ossia::vertex_format::uint16x2;
    case F::ushort4:    return ossia::vertex_format::uint16x4;
    case F::sshort1:    return ossia::vertex_format::sint16x1;
    case F::sshort2:    return ossia::vertex_format::sint16x2;
    case F::sshort4:    return ossia::vertex_format::sint16x4;
    default:            break;
  }
  return ossia::vertex_format::float3;
}

} // namespace

std::shared_ptr<ossia::scene_state> sceneStateFromMeshes(
    std::vector<Threedim::mesh> meshes,
    Threedim::float_vec buffer,
    std::string_view source_label)
{
  if(meshes.empty() || buffer.empty())
    return nullptr;

  // One CPU buffer shared across every mesh part. The buffer_resource holds
  // a shared_ptr<const void>; we stash the float_vec inside a shared_ptr
  // deleter to preserve its lifetime and keep the .data() address stable.
  // vertex_count == total element count across all attrs is irrelevant to
  // the consumer — each mesh_primitive carries its own per-primitive count.
  const int64_t buffer_bytes = (int64_t)(buffer.size() * sizeof(float));
  auto buf_owner = std::make_shared<Threedim::float_vec>(std::move(buffer));
  std::shared_ptr<const void> buf_handle(buf_owner, buf_owner->data());

  auto vertex_buf = std::make_shared<ossia::buffer_resource>();
  vertex_buf->resource = ossia::buffer_data{
      .data = std::move(buf_handle),
      .byte_size = buffer_bytes,
      .usage_hint = ossia::buffer_data::usage::vertex_buffer};
  vertex_buf->content_hash = (uint64_t)(uintptr_t)buf_owner->data();
  ossia::buffer_resource_ptr shared_buf{std::move(vertex_buf)};

  auto roots = std::make_shared<std::vector<ossia::scene_node_ptr>>();
  roots->reserve(meshes.size());

  for(std::size_t i = 0; i < meshes.size(); ++i)
  {
    const auto& m = meshes[i];
    if(m.vertices <= 0)
      continue;

    ossia::mesh_primitive prim;
    // Stable id keyed on the shared buffer pointer + index, matching
    // the scene_node id below. Required by the registry's mesh-slab
    // allocator: a 0 id makes the slab uncacheable and the mesh
    // disappears from rendering.
    prim.stable_id
        = (uint64_t)((uintptr_t)shared_buf.get()) ^ ((uint64_t)i + 1);
    prim.vertex_buffers.push_back(shared_buf);
    prim.vertex_count = (uint32_t)m.vertices;
    prim.topology = m.points ? ossia::primitive_topology::points
                             : ossia::primitive_topology::triangles;
    prim.index_type = ossia::index_format::none;

    // Local-space AABB over the position stream (tightly packed float3).
    // buf_owner owns the floats; m.pos_offset is the element offset to
    // the first position component. Enables per-draw GPU culling.
    {
      const float* positions = buf_owner->data() + m.pos_offset;
      prim.bounds = ossia::compute_aabb_from_positions(
          positions, (std::size_t)m.vertices);
    }

    // Byte-offset of each non-interleaved attribute block in the shared
    // vertex buffer. Convert element-offset (floats) to bytes.
    auto push_attr = [&](ossia::attribute_semantic sem,
                         ossia::vertex_format fmt, int64_t elem_offset,
                         uint32_t stride)
    {
      ossia::vertex_attribute a{};
      a.semantic     = sem;
      a.format       = fmt;
      a.buffer_index = 0;
      a.byte_offset  = (uint32_t)(elem_offset * (int64_t)sizeof(float));
      a.byte_stride  = stride;
      a.rate         = ossia::vertex_attribute::input_rate::per_vertex;
      prim.attributes.push_back(a);
    };

    push_attr(ossia::attribute_semantic::position,
              ossia::vertex_format::float3, m.pos_offset,
              3 * sizeof(float));
    if(m.normals)
      push_attr(ossia::attribute_semantic::normal,
                ossia::vertex_format::float3, m.normal_offset,
                3 * sizeof(float));
    if(m.texcoord)
      push_attr(ossia::attribute_semantic::texcoord0,
                ossia::vertex_format::float2, m.texcoord_offset,
                2 * sizeof(float));
    if(m.colors)
      push_attr(ossia::attribute_semantic::color0,
                ossia::vertex_format::float4, m.color_offset,
                4 * sizeof(float));
    if(m.tangents)
      push_attr(ossia::attribute_semantic::tangent,
                ossia::vertex_format::float4, m.tangent_offset,
                4 * sizeof(float));

    for(const auto& extra : m.extras)
    {
      auto sem = translateExtraSemantic(extra);
      auto fmt = translateFormat(extra.format, extra.components);
      const uint32_t stride = (uint32_t)(extra.components * sizeof(float));
      push_attr(sem, fmt, extra.offset, stride);
    }

    auto mesh_comp = std::make_shared<ossia::mesh_component>();
    mesh_comp->primitives.push_back(std::move(prim));

    auto children = std::make_shared<std::vector<ossia::scene_payload>>();
    children->push_back(ossia::mesh_component_ptr{std::move(mesh_comp)});

    auto node = std::make_shared<ossia::scene_node>();
    node->id.value = (uint64_t)((uintptr_t)shared_buf.get())
                     ^ ((uint64_t)i + 1);
    node->name = source_label.empty()
                     ? std::string("mesh_" + std::to_string(i))
                     : std::string(source_label);
    if(meshes.size() > 1)
    {
      node->name += '#';
      node->name += std::to_string(i);
    }
    node->children = std::move(children);
    roots->push_back(std::move(node));
  }

  if(roots->empty())
    return nullptr;

  auto state = std::make_shared<ossia::scene_state>();
  state->roots = std::move(roots);
  state->version = 1;
  state->dirty_index = 1;
  return state;
}

} // namespace Threedim
