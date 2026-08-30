// Coverage for the attribute-description layer every geometry extraction path
// in Threedim/ goes through: attributeFormatSize / attributeFormatComponents /
// isFloatFormat, the two findAttribute() overloads, and the attribute_lookup
// result they produce (including canDirectReference(), which is the whole of
// DirectReferenceStrategy's admission test).
//
// These are the only parts of GeometryToBufferStrategies.hpp that need no QRhi:
// everything past the lookup allocates GPU buffers. The lookup is also where a
// malformed dynamic_gpu_geometry (binding index past the bindings array, a
// binding with no matching input, an input pointing outside the buffer array)
// must be rejected rather than indexed — those three guards are what stop a
// producer node's bad descriptor from becoming an out-of-bounds read here.

#include <Threedim/GeometryToBufferStrategies.hpp>

#include <catch2/catch_test_macros.hpp>

using halp::attribute_format;
using halp::attribute_semantic;

namespace
{
// One buffer, one binding, one input, one attribute: the shape a single
// non-interleaved position stream has.
halp::dynamic_gpu_geometry makeSimple(int stride, int32_t attr_byte_offset)
{
  static int dummy = 0;
  halp::dynamic_gpu_geometry mesh;
  mesh.buffers.push_back(halp::geometry_gpu_buffer{.handle = &dummy, .byte_size = 4096});
  mesh.bindings.push_back(
      halp::geometry_binding{
          .stride = stride,
          .step_rate = 1,
          .classification = halp::binding_classification::per_vertex});
  mesh.input.push_back(halp::geometry_input{.buffer = 0, .byte_offset = 0});
  mesh.attributes.push_back(
      halp::geometry_attribute{
          .binding = 0,
          .semantic = attribute_semantic::position,
          .format = attribute_format::float3,
          .byte_offset = attr_byte_offset});
  mesh.vertices = 128;
  return mesh;
}
} // namespace

TEST_CASE("attributeFormatSize covers every format family", "[threedim][attrlookup]")
{
  CHECK(Threedim::attributeFormatSize(attribute_format::float4) == 16);
  CHECK(Threedim::attributeFormatSize(attribute_format::float3) == 12);
  CHECK(Threedim::attributeFormatSize(attribute_format::float2) == 8);
  CHECK(Threedim::attributeFormatSize(attribute_format::float1) == 4);

  CHECK(Threedim::attributeFormatSize(attribute_format::uint4) == 16);
  CHECK(Threedim::attributeFormatSize(attribute_format::sint1) == 4);

  CHECK(Threedim::attributeFormatSize(attribute_format::unormbyte4) == 4);
  CHECK(Threedim::attributeFormatSize(attribute_format::unormbyte2) == 2);
  CHECK(Threedim::attributeFormatSize(attribute_format::unormbyte1) == 1);

  CHECK(Threedim::attributeFormatSize(attribute_format::half4) == 8);
  CHECK(Threedim::attributeFormatSize(attribute_format::half3) == 6);
  CHECK(Threedim::attributeFormatSize(attribute_format::ushort2) == 4);
  CHECK(Threedim::attributeFormatSize(attribute_format::sshort1) == 2);
}

TEST_CASE(
    "attributeFormatSize of an unknown format is zero, not a wild read",
    "[threedim][attrlookup]")
{
  // A descriptor coming off a node that was compiled against a newer halp
  // could carry an enumerator this build does not know. It must fall through
  // to 0 (which every caller treats as "no attribute") instead of running off
  // the end of a jump table.
  const auto bogus = static_cast<attribute_format>(200);
  CHECK(Threedim::attributeFormatSize(bogus) == 0);
  CHECK(Threedim::attributeFormatComponents(bogus) == 0);
}

TEST_CASE("attributeFormatComponents counts lanes", "[threedim][attrlookup]")
{
  CHECK(Threedim::attributeFormatComponents(attribute_format::float4) == 4);
  CHECK(Threedim::attributeFormatComponents(attribute_format::float3) == 3);
  CHECK(Threedim::attributeFormatComponents(attribute_format::float2) == 2);
  CHECK(Threedim::attributeFormatComponents(attribute_format::float1) == 1);
  CHECK(Threedim::attributeFormatComponents(attribute_format::uint3) == 3);
  CHECK(Threedim::attributeFormatComponents(attribute_format::sshort2) == 2);
  CHECK(Threedim::attributeFormatComponents(attribute_format::unormbyte4) == 4);
  CHECK(Threedim::attributeFormatComponents(attribute_format::half1) == 1);
}

TEST_CASE(
    "isFloatFormat accepts float and half, rejects integer families",
    "[threedim][attrlookup]")
{
  // GeometryPacker only pads to vec4 when the source is float-ish; getting
  // this wrong silently reinterprets integer joint indices as floats.
  CHECK(Threedim::isFloatFormat(attribute_format::float4));
  CHECK(Threedim::isFloatFormat(attribute_format::float1));
  CHECK(Threedim::isFloatFormat(attribute_format::half4));
  CHECK(Threedim::isFloatFormat(attribute_format::half1));

  CHECK_FALSE(Threedim::isFloatFormat(attribute_format::unormbyte4));
  CHECK_FALSE(Threedim::isFloatFormat(attribute_format::uint4));
  CHECK_FALSE(Threedim::isFloatFormat(attribute_format::sint1));
  CHECK_FALSE(Threedim::isFloatFormat(attribute_format::ushort4));
  CHECK_FALSE(Threedim::isFloatFormat(attribute_format::sshort1));
}

TEST_CASE("findAttribute by semantic resolves the whole chain", "[threedim][attrlookup]")
{
  auto mesh = makeSimple(3 * sizeof(float), 0);
  auto lk = Threedim::findAttribute(mesh, attribute_semantic::position);
  REQUIRE(lk.has_value());
  CHECK(lk->valid());
  CHECK(lk->attribute == &mesh.attributes[0]);
  CHECK(lk->binding == &mesh.bindings[0]);
  CHECK(lk->input == &mesh.input[0]);
  CHECK(lk->buffer == &mesh.buffers[0]);
  CHECK(lk->attribute_size == 12);
  CHECK(lk->binding_index == 0);
}

TEST_CASE("findAttribute of an absent semantic is empty", "[threedim][attrlookup]")
{
  auto mesh = makeSimple(3 * sizeof(float), 0);
  CHECK_FALSE(Threedim::findAttribute(mesh, attribute_semantic::normal).has_value());
  CHECK_FALSE(Threedim::findAttribute(mesh, attribute_semantic::joints0).has_value());
}

TEST_CASE(
    "canDirectReference requires offset 0 and a tightly-packed binding",
    "[threedim][attrlookup]")
{
  SECTION("tightly packed, zero offset")
  {
    auto mesh = makeSimple(3 * sizeof(float), 0);
    auto lk = Threedim::findAttribute(mesh, attribute_semantic::position);
    REQUIRE(lk.has_value());
    CHECK(lk->canDirectReference());
  }

  SECTION("interleaved binding: stride is wider than the attribute")
  {
    // pos(12) + normal(12) + uv(8) interleaved into one 32-byte vertex.
    auto mesh = makeSimple(32, 0);
    auto lk = Threedim::findAttribute(mesh, attribute_semantic::position);
    REQUIRE(lk.has_value());
    CHECK_FALSE(lk->canDirectReference());
  }

  SECTION("non-zero attribute offset inside the binding")
  {
    auto mesh = makeSimple(3 * sizeof(float), 12);
    auto lk = Threedim::findAttribute(mesh, attribute_semantic::position);
    REQUIRE(lk.has_value());
    CHECK_FALSE(lk->canDirectReference());
  }
}

TEST_CASE(
    "a default-constructed attribute_lookup is invalid and never direct",
    "[threedim][attrlookup]")
{
  Threedim::attribute_lookup lk{};
  CHECK_FALSE(lk.valid());
  CHECK_FALSE(lk.canDirectReference());
}

TEST_CASE("findAttribute rejects malformed descriptors", "[threedim][attrlookup]")
{
  SECTION("binding index past the bindings array")
  {
    auto mesh = makeSimple(3 * sizeof(float), 0);
    mesh.attributes[0].binding = 7;
    CHECK_FALSE(Threedim::findAttribute(mesh, attribute_semantic::position).has_value());
  }

  SECTION("negative binding index")
  {
    auto mesh = makeSimple(3 * sizeof(float), 0);
    mesh.attributes[0].binding = -1;
    CHECK_FALSE(Threedim::findAttribute(mesh, attribute_semantic::position).has_value());
  }

  SECTION("a binding with no matching input entry")
  {
    // input[] is indexed by BINDING index, not by a separate input index:
    // two bindings with only one input means binding 1 has nothing to read.
    auto mesh = makeSimple(3 * sizeof(float), 0);
    mesh.bindings.push_back(mesh.bindings[0]);
    mesh.attributes[0].binding = 1;
    REQUIRE(mesh.input.size() == 1);
    CHECK_FALSE(Threedim::findAttribute(mesh, attribute_semantic::position).has_value());
  }

  SECTION("input pointing outside the buffers array")
  {
    auto mesh = makeSimple(3 * sizeof(float), 0);
    mesh.input[0].buffer = 4;
    CHECK_FALSE(Threedim::findAttribute(mesh, attribute_semantic::position).has_value());
  }

  SECTION("negative input buffer index")
  {
    auto mesh = makeSimple(3 * sizeof(float), 0);
    mesh.input[0].buffer = -1;
    CHECK_FALSE(Threedim::findAttribute(mesh, attribute_semantic::position).has_value());
  }

  SECTION("no attributes at all")
  {
    halp::dynamic_gpu_geometry mesh;
    CHECK_FALSE(Threedim::findAttribute(mesh, attribute_semantic::position).has_value());
  }
}

TEST_CASE("findAttribute by index agrees with the semantic overload",
          "[threedim][attrlookup]")
{
  static int dummy = 0;
  halp::dynamic_gpu_geometry mesh;
  mesh.buffers.push_back(halp::geometry_gpu_buffer{.handle = &dummy, .byte_size = 4096});
  for(int i = 0; i < 2; i++)
  {
    mesh.bindings.push_back(
        halp::geometry_binding{
            .stride = 3 * (int)sizeof(float),
            .step_rate = 1,
            .classification = halp::binding_classification::per_vertex});
    mesh.input.push_back(
        halp::geometry_input{.buffer = 0, .byte_offset = i * 1024});
  }
  mesh.attributes.push_back(
      halp::geometry_attribute{
          .binding = 0,
          .semantic = attribute_semantic::position,
          .format = attribute_format::float3});
  mesh.attributes.push_back(
      halp::geometry_attribute{
          .binding = 1,
          .semantic = attribute_semantic::normal,
          .format = attribute_format::float3});

  auto by_index = Threedim::findAttribute(mesh, 1);
  auto by_sem = Threedim::findAttribute(mesh, attribute_semantic::normal);
  REQUIRE(by_index.has_value());
  REQUIRE(by_sem.has_value());
  CHECK(by_index->attribute == by_sem->attribute);
  CHECK(by_index->binding_index == 1);
  CHECK(by_index->input->byte_offset == 1024);
}

TEST_CASE("findAttribute by index range-checks", "[threedim][attrlookup]")
{
  auto mesh = makeSimple(3 * sizeof(float), 0);
  CHECK_FALSE(Threedim::findAttribute(mesh, -1).has_value());
  CHECK_FALSE(Threedim::findAttribute(mesh, 1).has_value());
  CHECK_FALSE(Threedim::findAttribute(mesh, 1000).has_value());
  CHECK(Threedim::findAttribute(mesh, 0).has_value());
}
