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
