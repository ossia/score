#include "BufferToGeometry2.hpp"

#include "GeometryToBufferStrategies.hpp"

#include <Threedim/Debug.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <cstring>

namespace Threedim
{

namespace
{

[[nodiscard]] constexpr int32_t
attributeFormatSize(BuffersToGeometry2::AttributeFormat fmt) noexcept
{
  return Threedim::attributeFormatSize((halp::attribute_format)fmt);
}

[[nodiscard]] constexpr halp::attribute_format
toHalpFormat(BuffersToGeometry2::AttributeFormat fmt) noexcept
{
  return static_cast<halp::attribute_format>(fmt);
}

[[nodiscard]] constexpr halp::primitive_topology
toHalpTopology(BuffersToGeometry2::PrimitiveTopology t) noexcept
{
  switch(t)
  {
    case BuffersToGeometry2::Triangles:
      return halp::primitive_topology::triangles;
    case BuffersToGeometry2::TriangleStrip:
      return halp::primitive_topology::triangle_strip;
    case BuffersToGeometry2::TriangleFan:
      return halp::primitive_topology::triangle_fan;
    case BuffersToGeometry2::Lines:
      return halp::primitive_topology::lines;
    case BuffersToGeometry2::LineStrip:
      return halp::primitive_topology::line_strip;
    case BuffersToGeometry2::Points:
      return halp::primitive_topology::points;
  }
  return halp::primitive_topology::triangles;
}

[[nodiscard]] constexpr halp::cull_mode
toHalpCullMode(BuffersToGeometry2::CullMode c) noexcept
{
  switch(c)
  {
    case BuffersToGeometry2::None:
      return halp::cull_mode::none;
    case BuffersToGeometry2::Front:
      return halp::cull_mode::front;
    case BuffersToGeometry2::Back:
      return halp::cull_mode::back;
  }
  return halp::cull_mode::none;
}

[[nodiscard]] constexpr halp::front_face
toHalpFrontFace(BuffersToGeometry2::FrontFace f) noexcept
{
  switch(f)
  {
    case BuffersToGeometry2::CounterClockwise:
      return halp::front_face::counter_clockwise;
    case BuffersToGeometry2::Clockwise:
      return halp::front_face::clockwise;
  }
  return halp::front_face::counter_clockwise;
}

[[nodiscard]] constexpr halp::index_format
toHalpIndexFormat(BuffersToGeometry2::IndexFormat f) noexcept
{
  switch(f)
  {
    case BuffersToGeometry2::UInt16:
      return halp::index_format::uint16;
    case BuffersToGeometry2::UInt32:
      return halp::index_format::uint32;
  }
  return halp::index_format::uint32;
}

[[nodiscard]] constexpr halp::binding_classification
toHalpClassification(bool instanced) noexcept
{
  return instanced ? halp::binding_classification::per_instance
                   : halp::binding_classification::per_vertex;
}

// Resolve a user-provided semantic name to ossia::attribute_semantic.
// Uses case-insensitive name_to_semantic; if unrecognized, returns custom.
[[nodiscard]] int resolveSemanticFromName(const std::string& name) noexcept
{
  if(name.empty())
    return static_cast<int>(ossia::attribute_semantic::custom);

  auto sem = ossia::name_to_semantic(name);
  return static_cast<int>(sem);
}

} // anonymous namespace

BuffersToGeometry2::BuffersToGeometry2()
{
  // Initialize transform to identity
  std::memset(outputs.geometry.transform, 0, sizeof(outputs.geometry.transform));
  outputs.geometry.transform[0] = 1.0f;
  outputs.geometry.transform[5] = 1.0f;
  outputs.geometry.transform[10] = 1.0f;
  outputs.geometry.transform[15] = 1.0f;
}

void BuffersToGeometry2::operator()()
{
  auto& mesh = outputs.geometry.mesh;
  auto& out = outputs.geometry;

  // Collect input buffers into array for indexed access
  std::array<const halp::gpu_buffer*, 8> inputBuffers = {
      &inputs.buffer_0.buffer, &inputs.buffer_1.buffer, &inputs.buffer_2.buffer,
      &inputs.buffer_3.buffer, &inputs.buffer_4.buffer, &inputs.buffer_5.buffer,
      &inputs.buffer_6.buffer, &inputs.buffer_7.buffer,
  };

  // Helper to read attribute config by index
  struct AttributeConfig
  {
    bool enabled;
    int32_t buffer;
    int32_t offset;
    int32_t stride;
    AttributeFormat format;
    std::string semantic;
    bool instanced;
  };

  auto getAttributeConfig = [&](int i) -> AttributeConfig {
    switch(i)
    {
#define CASE(n)                                 \
  case n:                                       \
    return {                                    \
        inputs.attribute_buffer_##n.value >= 0, \
        inputs.attribute_buffer_##n.value,      \
        inputs.attribute_offset_##n.value,      \
        inputs.attribute_stride_##n.value,      \
        inputs.format_##n.value,                \
        inputs.semantic_##n.value,              \
        inputs.instanced_##n.value};
      CASE(0)
      CASE(1)
      CASE(2)
      CASE(3)
      CASE(4)
      CASE(5)
      CASE(6)
      CASE(7)
#undef CASE
      default:
        return {};
    }
  };

  // Check if anything changed
  bool meshChanged = false;
  bool buffersChanged = false;
  bool transformChanged = false;

  // Check transform changes
  transformChanged = true; // Simplified - compute properly based on your controls

  // Check mesh configuration changes
  if(inputs.vertices.value != m_prevVertices || inputs.topology.value != m_prevTopology
     || inputs.cull_mode.value != m_prevCullMode
     || inputs.front_face.value != m_prevFrontFace
     || inputs.index_buffer.value != m_prevUseIndexBuffer
     || ((inputs.index_buffer.value >= 0)
         && (inputs.index_format.value != m_prevIndexFormat
             || inputs.index_offset.value != m_prevIndexOffset)))
  {
    meshChanged = true;
  }

  // Check attribute changes
  for(int i = 0; i < 8; ++i)
  {
    auto cfg = getAttributeConfig(i);
    auto& prev = m_prevAttributes[i];

    if(cfg.enabled != prev.enabled || cfg.buffer != prev.buffer
       || cfg.offset != prev.offset || cfg.stride != prev.stride
       || cfg.format != prev.format || cfg.semantic != prev.semantic
       || cfg.instanced != prev.instanced)
    {
      meshChanged = true;
      prev = {cfg.enabled,  cfg.buffer, cfg.offset,   cfg.stride,
              cfg.format,   cfg.semantic, cfg.instanced};
    }
  }
  for(int i = 0; i < 8; ++i)
  {
    if(inputBuffers[i]->handle != m_prevBuffers[i].handle)
    {
      buffersChanged = true;
      m_prevBuffers[i] = *inputBuffers[i];
    }
  }

  // Update cached state
  m_prevVertices = inputs.vertices.value;
  m_prevTopology = inputs.topology.value;
  m_prevCullMode = inputs.cull_mode.value;
  m_prevFrontFace = inputs.front_face.value;
  m_prevUseIndexBuffer = inputs.index_buffer.value;
  m_prevIndexFormat = inputs.index_format.value;
  m_prevIndexOffset = inputs.index_offset.value;

  out.dirty_transform = transformChanged;

  if(!meshChanged && !buffersChanged)
  {
    out.dirty_mesh = false;
    for(auto& out_buf : out.mesh.buffers)
    {
      for(auto& in_buf : inputBuffers)
      {
        if(in_buf->handle == out_buf.handle)
        {
          out_buf.dirty = in_buf->changed;
          break;
        }
      }
    }
    return;
  }

  // Build the geometry
  mesh.buffers.clear();
  mesh.bindings.clear();
  mesh.attributes.clear();
  mesh.input.clear();

  // Track which input buffers are used and map them to output buffer indices
  std::array<int, 8> bufferMapping{};
  std::fill(bufferMapping.begin(), bufferMapping.end(), -1);

  // First pass: load buffers
  // If some buffers are missing, we ain't sending any geometry
  for(int i = 0; i < 8; ++i)
  {
    auto cfg = getAttributeConfig(i);
    if(!cfg.enabled)
      continue;

    const int bufIdx = cfg.buffer;
    if(bufIdx < 0 || bufIdx >= 8)
      continue;

    const auto* srcBuf = inputBuffers[bufIdx];
    if(!srcBuf || !srcBuf->handle)
    {
      // Null buffer somewhere
      m_prevVertices = -1; // to force reanalysis
      return;
    }
  }

  // Now we know we have good buffers
  for(int i = 0; i < 8; ++i)
  {
    auto cfg = getAttributeConfig(i);
    if(!cfg.enabled)
      continue;

    const int bufIdx = cfg.buffer;
    const auto* srcBuf = inputBuffers[bufIdx];

    // Check if this buffer is already added
    if(bufferMapping[bufIdx] < 0)
    {
      bufferMapping[bufIdx] = static_cast<int>(mesh.buffers.size());
      mesh.buffers.push_back(
          halp::geometry_gpu_buffer{
              .handle = srcBuf->handle, .byte_size = srcBuf->byte_size, .dirty = true});
    }
  }

  mesh.vertices = inputs.vertices.value;
  mesh.instances = inputs.instances.value;
  mesh.topology = toHalpTopology(inputs.topology.value);
  mesh.cull_mode = toHalpCullMode(inputs.cull_mode.value);
  mesh.front_face = toHalpFrontFace(inputs.front_face.value);

  // Second pass: create bindings, attributes, and inputs
  // Each enabled attribute gets its own binding (simplest approach)
  int attr_idx = 0;
  for(int i = 0; i < 8; ++i)
  {
    auto cfg = getAttributeConfig(i);
    if(!cfg.enabled)
      continue;

    const int bufIdx = cfg.buffer;
    if(bufIdx < 0 || bufIdx >= 8 || bufferMapping[bufIdx] < 0)
      continue;

    const auto* srcBuf = inputBuffers[bufIdx];
    if(!srcBuf || !srcBuf->handle)
      continue;

    const int bindingIndex = static_cast<int>(mesh.bindings.size());
    const int attrSize = attributeFormatSize(cfg.format);

    // If stride is 0, default to attribute size (tightly packed)
    const int stride = (cfg.stride > 0) ? cfg.stride : attrSize;

    // Add binding
    mesh.bindings.push_back(
        {.stride = stride,
         .step_rate = 1,
         .classification = toHalpClassification(cfg.instanced)});

    // Resolve semantic from user-provided name
    const int sem = resolveSemanticFromName(cfg.semantic);

    // Add attribute with sequential location and resolved semantic
    mesh.attributes.push_back(
        halp::geometry_attribute{
            .binding = bindingIndex,
            .semantic = static_cast<halp::attribute_semantic>(sem),
            .format = toHalpFormat(cfg.format),
            .byte_offset = 0, // Offset within stride is 0 since we use input offset
        });

    attr_idx++;

    // Add input (maps binding to buffer + offset)
    mesh.input.push_back(
        halp::geometry_input{
            .buffer = bufferMapping[bufIdx],
            .byte_offset
            = cfg.offset
              + srcBuf->byte_offset // Combine attribute offset with buffer view offset
        });
  }

  // Setup index buffer if enabled
  if(inputs.index_buffer.value >= 0 && inputs.index_buffer.value < 8)
  {
    const auto& idxBuf = *inputBuffers[inputs.index_buffer.value];
    if(idxBuf.handle && idxBuf.byte_size > 0)
    {
      // Add index buffer to buffers array
      const int idxBufIndex = static_cast<int>(mesh.buffers.size());
      mesh.buffers.push_back(
          halp::geometry_gpu_buffer{
              .handle = idxBuf.handle, .byte_size = idxBuf.byte_size, .dirty = true});

      mesh.index.buffer = idxBufIndex;
      mesh.index.byte_offset = inputs.index_offset.value + idxBuf.byte_offset;
      mesh.index.format = toHalpIndexFormat(inputs.index_format.value);
    }
    else
    {
      mesh.index.buffer = -1;
      mesh.index.byte_offset = 0;
    }
  }
  else
  {
    mesh.index.buffer = -1;
    mesh.index.byte_offset = 0;
  }

  out.dirty_mesh = meshChanged;
  out.dirty_transform = transformChanged;
}

} // namespace Threedim
