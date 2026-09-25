// Merge Geometries flips the tangent handedness under a mirroring transform.
//
// A tangent is vec4: xyz the direction, w the handedness of the tangent frame
// (bitangent = cross(N, T.xyz) * T.w, glTF TANGENT). The bake transformed xyz
// only; the determinant's sign reached the normal matrix but never w, so a
// negative-scale transform left every tangent frame with the wrong handedness.
// Both the CPU bake and the GPU compute bake must negate w when the transform's
// determinant is negative, and leave it alone otherwise.
//
// CPU: the merge renderer is driven directly (its RenderList is inert storage,
// as in MergeGeometriesTransformFixF.cpp). GPU: a CSF writes (1, 0, 0, +1)
// tangents into storage buffers, Merge Geometries bakes scale(-1, 1, 1) and a
// raw raster paints tangent.x as red and tangent.w as green.

#include "IsfTestCommon.hpp"

#include <Gfx/Graph/MergeGeometriesNode.hpp>
#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/Utils.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <memory>

using namespace score::test::gfx;
using namespace score::test::gfx::isf;
using Catch::Approx;

namespace
{
struct SinkRenderer final : score::gfx::NodeRenderer
{
  using NodeRenderer::NodeRenderer;
  void init(score::gfx::RenderList&, QRhiResourceUpdateBatch&) override { }
  void update(score::gfx::RenderList&, QRhiResourceUpdateBatch&, score::gfx::Edge*) override
  {
  }
  void release(score::gfx::RenderList&) override { }
  void removeOutputPass(score::gfx::RenderList&, score::gfx::Edge&) override { }
};

struct SinkNode final : score::gfx::Node
{
  SinkNode()
  {
    input.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Geometry, {}});
  }
  score::gfx::NodeRenderer* createRenderer(score::gfx::RenderList&) const noexcept override
  {
    return nullptr;
  }
};

// One triangle: float3 positions (binding 0), then float4 tangents
// (1, 0, 0, +1) (binding 1), in one buffer.
ossia::geometry_spec makeTriangle()
{
  const float data[21] = {
      0, 0, 0, 1, 0, 0, 0, 1, 0,             // positions
      1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1,    // tangents
  };
  auto* bytes = new unsigned char[sizeof(data)];
  std::memcpy(bytes, data, sizeof(data));

  ossia::geometry g;
  g.buffers.push_back(
      {ossia::geometry::cpu_buffer{
           std::shared_ptr<void>(bytes, [](void* p) { delete[] (unsigned char*)p; }),
           int64_t(sizeof(data))},
       true});
  g.bindings.push_back({12, ossia::geometry::binding::per_vertex, 0});
  g.bindings.push_back({16, ossia::geometry::binding::per_vertex, 0});
  ossia::geometry::attribute pos;
  pos.binding = 0;
  pos.location = 0;
  pos.format = ossia::geometry::attribute::float3;
  pos.semantic = ossia::attribute_semantic::position;
  ossia::geometry::attribute tan = pos;
  tan.binding = 1;
  tan.location = 1;
  tan.format = ossia::geometry::attribute::float4;
  tan.semantic = ossia::attribute_semantic::tangent;
  g.attributes.push_back(pos);
  g.attributes.push_back(tan);
  g.input.push_back({0, 0});
  g.input.push_back({0, 36});
  g.vertices = 3;
  g.topology = ossia::geometry::triangles;
  g.cull_mode = ossia::geometry::none;
  g.front_face = ossia::geometry::counter_clockwise;
  g.bounds = {{0, 0, 0}, {1, 1, 0}};

  ossia::geometry_spec spec;
  spec.meshes = std::make_shared<ossia::mesh_list>();
  spec.meshes->meshes.push_back(std::move(g));
  spec.filters = std::make_shared<ossia::geometry_filter_list>();
  return spec;
}

ossia::transform3d scaled(float sx, float sy, float sz)
{
  ossia::transform3d t;
  const float m[16] = {sx, 0, 0, 0, 0, sy, 0, 0, 0, 0, sz, 0, 0, 0, 0, 1};
  std::copy_n(m, 16, t.matrix);
  return t;
}

// Tangents of the baked output: 3 x (x, y, z, w).
std::array<float, 12> bakeTangents(const ossia::transform3d& t)
{
  alignas(64) static unsigned char rl_storage[16384]{};
  alignas(64) static unsigned char misc_storage[4096]{};
  auto& rl = *reinterpret_cast<score::gfx::RenderList*>(rl_storage);
  auto& batch = *reinterpret_cast<QRhiResourceUpdateBatch*>(misc_storage);
  auto& cb = *reinterpret_cast<QRhiCommandBuffer*>(misc_storage);
  QRhiResourceUpdateBatch* batch_ptr = &batch;

  score::gfx::MergeGeometriesNode merge;
  std::unique_ptr<score::gfx::NodeRenderer> r{merge.createRenderer(rl)};
  REQUIRE(r);

  SinkNode sinkNode;
  SinkRenderer sink{sinkNode};
  sinkNode.renderedNodes[&rl] = &sink;
  score::gfx::Edge edge{
      merge.output[0], sinkNode.input[0], Process::CableType::ImmediateGlutton};

  const auto a = makeTriangle();
  const char keyA{};
  r->process(0, a, &keyA);
  r->process(0, t);
  r->update(rl, batch, nullptr);
  r->runInitialPasses(rl, cb, batch_ptr, edge);

  std::array<float, 12> out{};
  const auto* spec = sink.findGeometryByPort(0);
  REQUIRE(spec);
  REQUIRE(spec->meshes);
  REQUIRE(spec->meshes->meshes.size() == 1);
  const auto& g = spec->meshes->meshes[0];
  auto* cpu = ossia::get_if<ossia::geometry::cpu_buffer>(&g.buffers[0].data);
  REQUIRE(cpu);
  std::memcpy(
      out.data(), static_cast<const unsigned char*>(cpu->raw_data.get()) + 36,
      sizeof(out));

  r->release(rl);
  sinkNode.renderedNodes.clear();
  return out;
}

struct Shot
{
  bool skipped{};
  std::string skip_reason;
  std::string backend;
  std::string error;
  ReadbackImage image;
};

Shot renderMerged(score::gfx::GraphicsApi api, const ossia::transform3d& transform)
{
  Shot out;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int producer = p.addIsf(corpus("mergetan-f2-producer.cs"));
    const int merge = p.addNode(std::make_unique<score::gfx::MergeGeometriesNode>());
    const int raster
        = p.addRaster(corpus("mergetan-f2-raster.vs"), corpus("mergetan-f2-raster.fs"));
    const int sink = p.addSink({64, 64});
    p.wire(p.geometryOut(producer, 0), p.nodeGeometryIn(merge, 0));
    p.wire(p.nodeGeometryOut(merge, 0), p.geometryIn(raster, 0));
    p.wire(p.imageOut(raster, 0), p.sinkInput(sink));

    if(!p.create(api))
    {
      out.skipped = p.skipped();
      out.skip_reason = p.skipReason();
      out.error = p.error();
      out.backend = p.backend();
      return;
    }
    out.backend = p.backend();

    auto* mergeNode = p.node(merge);
    for(int f = 0; f < 6; f++)
    {
      for(auto& [rl, r] : mergeNode->renderedNodes)
        r->process(0, transform);
      p.render(1);
    }
    out.image = p.readback(sink);
    out.error = p.error();
  });
  return out;
}

bool nearRg(std::array<uint8_t, 4> px, int r, int g, int tol = 6)
{
  return std::abs(px[0] - r) <= tol && std::abs(px[1] - g) <= tol;
}
}

TEST_CASE(
    "Merge Geometries CPU bake flips tangent handedness under a mirror",
    "[gfx][merge][tangent][f2]")
{
  SECTION("a mirroring transform negates w")
  {
    const auto t = bakeTangents(scaled(-2.f, 1.f, 1.f));
    for(int v = 0; v < 3; v++)
    {
      CHECK(t[4 * v + 0] == Approx(-1.f));
      CHECK(t[4 * v + 1] == Approx(0.f).margin(1e-6));
      CHECK(t[4 * v + 2] == Approx(0.f).margin(1e-6));
      CHECK(t[4 * v + 3] == Approx(-1.f));
    }
  }

  SECTION("a non-mirroring transform keeps w")
  {
    const auto t = bakeTangents(scaled(-2.f, -1.f, 1.f));
    for(int v = 0; v < 3; v++)
    {
      CHECK(t[4 * v + 0] == Approx(-1.f));
      CHECK(t[4 * v + 3] == Approx(1.f));
    }
  }
}

TEST_CASE(
    "Merge Geometries GPU bake flips tangent handedness under a mirror",
    "[gfx][merge][tangent][gpu][f2]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  SECTION("identity keeps the tangent")
  {
    const auto shot = renderMerged(backend, ossia::transform3d{});
    if(shot.skipped)
      SKIP(shot.backend + ": " + shot.skip_reason);
    if(const char* why = compute_shader_skip_reason(backend))
      SKIP(why);
    REQUIRE(shot.error.empty());
    REQUIRE(shot.image.valid());
    const auto c = shot.image.at(32, 32);
    INFO("centre=" << int(c[0]) << "," << int(c[1]));
    CHECK(nearRg(c, 255, 255));
  }

  SECTION("scale(-1, 1, 1) mirrors x and negates w")
  {
    const auto shot = renderMerged(backend, scaled(-1.f, 1.f, 1.f));
    if(shot.skipped)
      SKIP(shot.backend + ": " + shot.skip_reason);
    if(const char* why = compute_shader_skip_reason(backend))
      SKIP(why);
    REQUIRE(shot.error.empty());
    REQUIRE(shot.image.valid());
    const auto c = shot.image.at(32, 32);
    INFO("centre=" << int(c[0]) << "," << int(c[1]));
    CHECK(nearRg(c, 0, 0));
  }
}
