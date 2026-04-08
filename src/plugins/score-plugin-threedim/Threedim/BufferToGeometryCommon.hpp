#pragma once

#include "GeometryToBufferStrategies.hpp"

#include <ossia/dataflow/geometry_port.hpp>

#include <Threedim/Debug.hpp>

#include <cstring>

namespace Threedim
{

// Matches QRhiVertexInputAttribute::Format
enum AttributeFormat
{
  Float4,
  Float3,
  Float2,
  Float,
  UNormByte4,
  UNormByte2,
  UNormByte,
  UInt4,
  UInt2,
  UInt,
  SInt4,
  SInt2,
  SInt,
  Half4,
  Half3,
  Half2,
  Half,
  UShort4,
  UShort2,
  UShort,
  SShort4,
  SShort2,
  SShort,
};

enum PrimitiveTopology
{
  Triangles,
  TriangleStrip,
  TriangleFan,
  Lines,
  LineStrip,
  Points
};

enum CullMode
{
  None,
  Front,
  Back
};

enum FrontFace
{
  CounterClockwise,
  Clockwise
};

enum IndexFormat
{
  UInt16,
  UInt32
};

namespace
{

[[nodiscard]] constexpr int32_t attributeFormatSize(AttributeFormat fmt) noexcept
{
  return Threedim::attributeFormatSize((halp::attribute_format)fmt);
}

[[nodiscard]] constexpr halp::attribute_format toHalpFormat(AttributeFormat fmt) noexcept
{
  return static_cast<halp::attribute_format>(fmt);
}

[[nodiscard]] constexpr halp::primitive_topology
toHalpTopology(PrimitiveTopology t) noexcept
{
  switch(t)
  {
    case PrimitiveTopology::Triangles:
      return halp::primitive_topology::triangles;
    case PrimitiveTopology::TriangleStrip:
      return halp::primitive_topology::triangle_strip;
    case PrimitiveTopology::TriangleFan:
      return halp::primitive_topology::triangle_fan;
    case PrimitiveTopology::Lines:
      return halp::primitive_topology::lines;
    case PrimitiveTopology::LineStrip:
      return halp::primitive_topology::line_strip;
    case PrimitiveTopology::Points:
      return halp::primitive_topology::points;
  }
  return halp::primitive_topology::triangles;
}

[[nodiscard]] constexpr halp::cull_mode toHalpCullMode(CullMode c) noexcept
{
  switch(c)
  {
    case CullMode::None:
      return halp::cull_mode::none;
    case CullMode::Front:
      return halp::cull_mode::front;
    case CullMode::Back:
      return halp::cull_mode::back;
  }
  return halp::cull_mode::none;
}

[[nodiscard]] constexpr halp::front_face toHalpFrontFace(FrontFace f) noexcept
{
  switch(f)
  {
    case FrontFace::CounterClockwise:
      return halp::front_face::counter_clockwise;
    case FrontFace::Clockwise:
      return halp::front_face::clockwise;
  }
  return halp::front_face::counter_clockwise;
}

[[nodiscard]] constexpr halp::index_format toHalpIndexFormat(IndexFormat f) noexcept
{
  switch(f)
  {
    case IndexFormat::UInt16:
      return halp::index_format::uint16;
    case IndexFormat::UInt32:
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
[[nodiscard]] inline int resolveSemanticFromName(const std::string& name) noexcept
{
  if(name.empty())
    return static_cast<int>(ossia::attribute_semantic::custom);

  auto sem = ossia::name_to_semantic(name);
  return static_cast<int>(sem);
}

} // anonymous namespace

}