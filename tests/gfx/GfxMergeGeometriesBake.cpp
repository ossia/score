// bakeGeometryTransform: the CPU half of Merge Geometries' per-input
// transform, on a mirroring transform where every rule shows.
//
//   position  model matrix (translation included)
//   normal    inverse-transpose of the upper 3x3, renormalised
//   tangent   upper 3x3, renormalised; its w (handedness) flips when the
//             transform mirrors (negative determinant)
//   bounds    the transformed box's axis-aligned hull
//
// The input buffers are never written: the baked meshes point to copies, and
// a buffer two meshes share is transformed once. The identity transform
// shares the input's buffers untouched.
#include <Gfx/Graph/MergeGeometriesNode.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <memory>

namespace
{
// Interleaved: position float3, normal float3, tangent float4 (40 bytes).
constexpr float kVertices[2][10] = {
    {1.f, 0.f, 0.f, 1.f, 0.f, 0.f, 1.f, 0.f, 0.f, 1.f},
    {0.f, 1.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f, 1.f},
};

std::shared_ptr<void> makeBuffer()
{
  auto data = std::shared_ptr<void>(new char[sizeof(kVertices)], [](void* p) {
    delete[] static_cast<char*>(p);
  });
  std::memcpy(data.get(), kVertices, sizeof(kVertices));
  return data;
}

ossia::geometry makeMesh(std::shared_ptr<void> data)
{
  ossia::geometry g;
  g.vertices = 2;
  g.buffers.push_back(
      {.data = ossia::geometry::cpu_buffer{std::move(data), sizeof(kVertices)}});
  g.bindings.push_back({.byte_stride = sizeof(kVertices[0])});
  g.input.push_back({.buffer = 0, .byte_offset = 0});

  auto attr = [&](ossia::attribute_semantic s, decltype(ossia::geometry::attribute::format) f,
                  uint32_t offset, int location) {
    ossia::geometry::attribute a;
    a.binding = 0;
    a.location = location;
    a.format = f;
    a.byte_offset = offset;
    a.semantic = s;
    g.attributes.push_back(a);
  };
  attr(ossia::attribute_semantic::position, ossia::geometry::attribute::float3, 0, 0);
  attr(ossia::attribute_semantic::normal, ossia::geometry::attribute::float3, 12, 1);
  attr(ossia::attribute_semantic::tangent, ossia::geometry::attribute::float4, 24, 2);

  g.bounds.min[0] = g.bounds.min[1] = g.bounds.min[2] = 0.f;
  g.bounds.max[0] = g.bounds.max[1] = 1.f;
  g.bounds.max[2] = 0.f;
  return g;
}

// Scale x by -2 (mirrors), translate by (1, 2, 3); column-major.
ossia::transform3d mirrorTranslate()
{
  ossia::transform3d t;
  t.matrix[0] = -2.f;
  t.matrix[12] = 1.f;
  t.matrix[13] = 2.f;
  t.matrix[14] = 3.f;
  return t;
}

const float* vertex(const ossia::geometry& g, int v)
{
  auto* cpu = ossia::get_if<ossia::geometry::cpu_buffer>(&g.buffers[0].data);
  REQUIRE(cpu);
  return static_cast<const float*>(cpu->raw_data.get()) + v * 10;
}

void checkBaked(const float* v, std::initializer_list<float> expected)
{
  int i = 0;
  for(float e : expected)
  {
    INFO("component " << i);
    CHECK(v[i] == Catch::Approx(e).margin(1e-6));
    ++i;
  }
}
}

TEST_CASE("a mirroring transform bakes positions, normals, tangents and bounds",
          "[gfx][merge-geometries]")
{
  auto data = makeBuffer();
  const std::vector<ossia::geometry> in{makeMesh(data)};

  const auto out = score::gfx::bakeGeometryTransform(in, mirrorTranslate());
  REQUIRE(out.size() == 1);

  checkBaked(vertex(out[0], 0), {-1.f, 2.f, 3.f, -1.f, 0.f, 0.f, -1.f, 0.f, 0.f, -1.f});
  checkBaked(vertex(out[0], 1), {1.f, 3.f, 3.f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f, -1.f});
  CHECK(out[0].buffers[0].dirty);

  CHECK(out[0].bounds.min[0] == Catch::Approx(-1.f));
  CHECK(out[0].bounds.min[1] == Catch::Approx(2.f));
  CHECK(out[0].bounds.min[2] == Catch::Approx(3.f));
  CHECK(out[0].bounds.max[0] == Catch::Approx(1.f));
  CHECK(out[0].bounds.max[1] == Catch::Approx(3.f));
  CHECK(out[0].bounds.max[2] == Catch::Approx(3.f));

  // The input is untouched.
  CHECK(std::memcmp(data.get(), kVertices, sizeof(kVertices)) == 0);
  CHECK(vertex(in[0], 0) != vertex(out[0], 0));
}

TEST_CASE("a buffer shared by two meshes is baked once", "[gfx][merge-geometries]")
{
  auto data = makeBuffer();
  const std::vector<ossia::geometry> in{makeMesh(data), makeMesh(data)};

  const auto out = score::gfx::bakeGeometryTransform(in, mirrorTranslate());
  REQUIRE(out.size() == 2);
  CHECK(vertex(out[0], 0) == vertex(out[1], 0));
  checkBaked(vertex(out[1], 0), {-1.f, 2.f, 3.f});
}

TEST_CASE("the identity transform shares the input buffers", "[gfx][merge-geometries]")
{
  auto data = makeBuffer();
  const std::vector<ossia::geometry> in{makeMesh(data)};

  const auto out = score::gfx::bakeGeometryTransform(in, ossia::transform3d{});
  REQUIRE(out.size() == 1);
  CHECK(vertex(out[0], 0) == vertex(in[0], 0));
  CHECK_FALSE(out[0].buffers[0].dirty);
}
