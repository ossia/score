#include "BufferToGeometry.hpp"

#include "GeometryToBufferStrategies.hpp"

#include <Threedim/Debug.hpp>

#include <cstring>

namespace Threedim
{

namespace
{

[[nodiscard]] constexpr int32_t
attributeFormatSize(BuffersToGeometry::AttributeFormat fmt) noexcept
{
  return Threedim::attributeFormatSize((halp::attribute_format)fmt);
}

[[nodiscard]] constexpr halp::attribute_format
toHalpFormat(BuffersToGeometry::AttributeFormat fmt) noexcept
{
  return static_cast<halp::attribute_format>(fmt);
}

[[nodiscard]] constexpr halp::primitive_topology
toHalpTopology(BuffersToGeometry::PrimitiveTopology t) noexcept
{
  switch(t)
  {
    case BuffersToGeometry::Triangles:
      return halp::primitive_topology::triangles;
    case BuffersToGeometry::TriangleStrip:
      return halp::primitive_topology::triangle_strip;
    case BuffersToGeometry::TriangleFan:
      return halp::primitive_topology::triangle_fan;
    case BuffersToGeometry::Lines:
      return halp::primitive_topology::lines;
    case BuffersToGeometry::LineStrip:
      return halp::primitive_topology::line_strip;
    case BuffersToGeometry::Points:
      return halp::primitive_topology::points;
  }
  return halp::primitive_topology::triangles;
}

[[nodiscard]] constexpr halp::cull_mode
toHalpCullMode(BuffersToGeometry::CullMode c) noexcept
{
  switch(c)
  {
    case BuffersToGeometry::None:
      return halp::cull_mode::none;
    case BuffersToGeometry::Front:
      return halp::cull_mode::front;
    case BuffersToGeometry::Back:
      return halp::cull_mode::back;
  }
  return halp::cull_mode::none;
}

[[nodiscard]] constexpr halp::front_face
toHalpFrontFace(BuffersToGeometry::FrontFace f) noexcept
{
  switch(f)
  {
    case BuffersToGeometry::CounterClockwise:
      return halp::front_face::counter_clockwise;
    case BuffersToGeometry::Clockwise:
      return halp::front_face::clockwise;
  }
  return halp::front_face::counter_clockwise;
}

[[nodiscard]] constexpr halp::index_format
toHalpIndexFormat(BuffersToGeometry::IndexFormat f) noexcept
{
  switch(f)
  {
    case BuffersToGeometry::UInt16:
      return halp::index_format::uint16;
    case BuffersToGeometry::UInt32:
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

// Convert location int to attribute_location enum
[[nodiscard]] constexpr halp::attribute_location toHalpLocation(int32_t loc) noexcept
{
  // Standard locations
  switch(loc)
  {
    case 0:
      return halp::attribute_location::position;
    case 1:
      return halp::attribute_location::tex_coord;
    case 2:
      return halp::attribute_location::color;
    case 3:
      return halp::attribute_location::normal;
    case 4:
      return halp::attribute_location::tangent;
    default:
      return static_cast<halp::attribute_location>(loc);
  }
}

} // anonymous namespace

BuffersToGeometry::BuffersToGeometry()
{
  // Initialize transform to identity
  std::memset(outputs.geometry.transform, 0, sizeof(outputs.geometry.transform));
  outputs.geometry.transform[0] = 1.0f;
  outputs.geometry.transform[5] = 1.0f;
  outputs.geometry.transform[10] = 1.0f;
  outputs.geometry.transform[15] = 1.0f;
}

void BuffersToGeometry::operator()()
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
    int32_t location;
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
        inputs.location_##n.value,              \
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
  bool transformChanged = false;

  // Check transform changes
  // (Assuming PositionControl, RotationControl, ScaleControl have .value members)
  // You'll need to compute the transform matrix and compare
  // For now, mark as changed if any transform input changed
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
       || cfg.format != prev.format || cfg.location != prev.location
       || cfg.instanced != prev.instanced)
    {
      meshChanged = true;
      prev = {cfg.enabled, cfg.buffer,   cfg.offset,   cfg.stride,
              cfg.format,  cfg.location, cfg.instanced};
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

  if(!meshChanged && !transformChanged)
  {
    out.dirty_mesh = false;
    out.dirty_transform = false;
    return;
  }

  // Build the geometry
  mesh.buffers.clear();
  mesh.bindings.clear();
  mesh.attributes.clear();
  mesh.input.clear();

  mesh.vertices = inputs.vertices.value;
  mesh.instances = inputs.instances.value;
  // FIXME indices if indexed
  mesh.topology = toHalpTopology(inputs.topology.value);
  mesh.cull_mode = toHalpCullMode(inputs.cull_mode.value);
  mesh.front_face = toHalpFrontFace(inputs.front_face.value);

  // Track which input buffers are used and map them to output buffer indices
  std::array<int, 8> bufferMapping{};
  std::fill(bufferMapping.begin(), bufferMapping.end(), -1);

  // First pass: collect unique buffers
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
      continue;

    // Check if this buffer is already added
    if(bufferMapping[bufIdx] < 0)
    {
      bufferMapping[bufIdx] = static_cast<int>(mesh.buffers.size());
      mesh.buffers.push_back(
          halp::geometry_gpu_buffer{
              .handle = srcBuf->handle, .byte_size = srcBuf->byte_size, .dirty = true});
    }
  }

  // Second pass: create bindings, attributes, and inputs
  // Each enabled attribute gets its own binding (simplest approach)
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

    // Add attribute
    mesh.attributes.push_back(
        halp::geometry_attribute{
            .binding = bindingIndex,
            .location = toHalpLocation(cfg.location),
            .format = toHalpFormat(cfg.format),
            .byte_offset = 0 // Offset within stride is 0 since we use input offset
        });

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
