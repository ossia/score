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

[[nodiscard]] constexpr halp::attribute_format toHalpFormat(AttributeFormat fmt) noexcept
{
  // A real switch, NOT a static_cast: the local enum mirrors
  // QRhiVertexInputAttribute::Format, which has no 3-component
  // int/short entries, while halp::attribute_format interleaves
  // uint3/sint3/ushort3/sshort3 into the sequence. The two enums agree
  // only for values 0..7 (Float4..UInt4); from UInt2 upward a cast
  // lands every format on the wrong halp entry (UInt2 -> uint3,
  // SInt4 -> uint1, UShort4 -> half2, ...).
  switch(fmt)
  {
    case AttributeFormat::Float4:     return halp::attribute_format::float4;
    case AttributeFormat::Float3:     return halp::attribute_format::float3;
    case AttributeFormat::Float2:     return halp::attribute_format::float2;
    case AttributeFormat::Float:      return halp::attribute_format::float1;
    case AttributeFormat::UNormByte4: return halp::attribute_format::unormbyte4;
    case AttributeFormat::UNormByte2: return halp::attribute_format::unormbyte2;
    case AttributeFormat::UNormByte:  return halp::attribute_format::unormbyte1;
    case AttributeFormat::UInt4:      return halp::attribute_format::uint4;
    case AttributeFormat::UInt2:      return halp::attribute_format::uint2;
    case AttributeFormat::UInt:       return halp::attribute_format::uint1;
    case AttributeFormat::SInt4:      return halp::attribute_format::sint4;
    case AttributeFormat::SInt2:      return halp::attribute_format::sint2;
    case AttributeFormat::SInt:       return halp::attribute_format::sint1;
    case AttributeFormat::Half4:      return halp::attribute_format::half4;
    case AttributeFormat::Half3:      return halp::attribute_format::half3;
    case AttributeFormat::Half2:      return halp::attribute_format::half2;
    case AttributeFormat::Half:       return halp::attribute_format::half1;
    case AttributeFormat::UShort4:    return halp::attribute_format::ushort4;
    case AttributeFormat::UShort2:    return halp::attribute_format::ushort2;
    case AttributeFormat::UShort:     return halp::attribute_format::ushort1;
    case AttributeFormat::SShort4:    return halp::attribute_format::sshort4;
    case AttributeFormat::SShort2:    return halp::attribute_format::sshort2;
    case AttributeFormat::SShort:     return halp::attribute_format::sshort1;
  }
  return halp::attribute_format::float4;
}

[[nodiscard]] constexpr int32_t attributeFormatSize(AttributeFormat fmt) noexcept
{
  return Threedim::attributeFormatSize(toHalpFormat(fmt));
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