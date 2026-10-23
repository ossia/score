// The vertex format a CSF publishes for each geometry attribute type (shader
// board C42): int / ivecN are signed. They were published with the unsigned
// formats, so a consumer reading them as signed got the bit patterns of the
// wrong type, and a negative value became a large positive one.
#include <Gfx/Graph/RenderedCSFNode.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("a CSF attribute's published format follows its GLSL type", "[gfx][csf][C42]")
{
  using A = ossia::geometry::attribute;
  using score::gfx::csfAttributeFormat;
  CHECK(csfAttributeFormat("float") == A::float1);
  CHECK(csfAttributeFormat("vec2") == A::float2);
  CHECK(csfAttributeFormat("vec3") == A::float3);
  CHECK(csfAttributeFormat("vec4") == A::float4);
  CHECK(csfAttributeFormat("int") == A::sint1);
  CHECK(csfAttributeFormat("ivec2") == A::sint2);
  CHECK(csfAttributeFormat("ivec3") == A::sint3);
  CHECK(csfAttributeFormat("ivec4") == A::sint4);
  CHECK(csfAttributeFormat("uint") == A::uint1);
  CHECK(csfAttributeFormat("uvec2") == A::uint2);
  CHECK(csfAttributeFormat("uvec3") == A::uint3);
  CHECK(csfAttributeFormat("uvec4") == A::uint4);
}

// Points 2-5: a type with no vertex format is published as user_struct with
// its name and std430 element size, and read only as that type.
namespace
{
isf::descriptor packetDescriptor()
{
  isf::descriptor d;
  isf::descriptor::type_definition packet;
  packet.name = "Packet";
  packet.layout = {{"value", "vec4"}, {"tag", "float"}};
  d.types.push_back(packet);
  isf::descriptor::type_definition other = packet;
  other.name = "Other";
  d.types.push_back(other);
  return d;
}
}

TEST_CASE("a CSF attribute with no vertex format is described as an element", "[gfx][csf][C42]")
{
  using A = ossia::geometry::attribute;
  const auto d = packetDescriptor();
  const auto describe = [&](std::string_view type) {
    A a;
    score::gfx::describeCsfAttribute(a, type, d);
    return a;
  };
  CHECK(describe("ivec2").format == A::sint2);
  CHECK(describe("ivec2").user_type_name.empty());
  CHECK(describe("vec4").format == A::float4);

  const auto b = describe("bool");
  CHECK(b.format == A::user_struct);
  CHECK(b.user_type_name == "bool");
  CHECK(b.element_byte_size == 4);
  CHECK(describe("mat4").element_byte_size == 64);
  CHECK(describe("mat3").element_byte_size == 48);
  const auto p = describe("Packet");
  CHECK(p.format == A::user_struct);
  CHECK(p.user_type_name == "Packet");
  CHECK(p.element_byte_size == 32);
}

TEST_CASE("a CSF attribute is read only as a type that matches it", "[gfx][csf][C42]")
{
  using A = ossia::geometry::attribute;
  using score::gfx::csfAttributeRefusal;
  const auto d = packetDescriptor();
  A packet;
  score::gfx::describeCsfAttribute(packet, "Packet", d);
  A vec;
  vec.format = A::float3;
  A unnamed;
  unnamed.format = A::user_struct;
  unnamed.element_byte_size = 32;

  CHECK(csfAttributeRefusal(packet, "Packet", d).isEmpty());
  CHECK_FALSE(csfAttributeRefusal(packet, "Other", d).isEmpty());
  CHECK_FALSE(csfAttributeRefusal(packet, "vec4", d).isEmpty());
  CHECK_FALSE(csfAttributeRefusal(unnamed, "Packet", d).isEmpty());
  // Scalars and vectors convert between each other (C40), not into elements.
  CHECK(csfAttributeRefusal(vec, "vec4", d).isEmpty());
  CHECK(csfAttributeRefusal(vec, "ivec3", d).isEmpty());
  CHECK_FALSE(csfAttributeRefusal(vec, "mat4", d).isEmpty());
  CHECK_FALSE(csfAttributeRefusal(vec, "bool", d).isEmpty());
  CHECK(csfAttributeRefusal(packet, "Other", d).contains(u"Packet"));
}
