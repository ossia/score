// Merge Geometries keeps each input's transform (N79a).
//
// A primitive's Position/Rotation/Scale reaches the next node as a
// transform3d on the edge, next to its geometry_spec. MergeGeometriesNode used
// to concatenate the meshes and drop those transforms, so every input landed at
// the origin at full size. The merged output must carry each input's CPU
// positions and normals in that input's transformed space, leave identity
// inputs sharing their buffers, and leave the upstream buffers untouched.
//
// CPU only: the merge renderer never touches the RenderList or QRhi objects it
// is handed; they are inert storage used as identity keys.

#include <Gfx/Graph/MergeGeometriesNode.hpp>
#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/Utils.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <memory>

using namespace score::gfx;
using Catch::Approx;

namespace
{
struct SinkRenderer final : NodeRenderer
{
  using NodeRenderer::NodeRenderer;
  void init(RenderList&, QRhiResourceUpdateBatch&) override { }
  void update(RenderList&, QRhiResourceUpdateBatch&, Edge*) override { }
  void release(RenderList&) override { }
  void removeOutputPass(RenderList&, Edge&) override { }
};

struct SinkNode final : Node
{
  SinkNode() { input.push_back(new Port{this, {}, Types::Geometry, {}}); }
  NodeRenderer* createRenderer(RenderList&) const noexcept override { return nullptr; }
};

// One triangle: positions (binding 0) then normals (binding 1), one buffer.
ossia::geometry_spec makeTriangle()
{
  const float data[18] = {
      0, 0, 0, 1, 0, 0, 0, 1, 0, // positions
      0, 0, 1, 0, 0, 1, 0, 0, 1, // normals
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
  g.bindings.push_back({12, ossia::geometry::binding::per_vertex, 0});
  ossia::geometry::attribute pos;
  pos.binding = 0;
  pos.location = 0;
  pos.format = ossia::geometry::attribute::float3;
  pos.semantic = ossia::attribute_semantic::position;
  ossia::geometry::attribute nrm = pos;
  nrm.binding = 1;
  nrm.location = 1;
  nrm.semantic = ossia::attribute_semantic::normal;
  g.attributes.push_back(pos);
  g.attributes.push_back(nrm);
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

const float* floats(const ossia::geometry& g)
{
  auto* cpu = ossia::get_if<ossia::geometry::cpu_buffer>(&g.buffers[0].data);
  REQUIRE(cpu);
  return static_cast<const float*>(cpu->raw_data.get());
}

// Column-major translate(tx, 0, 0) * rotateZ(90 deg) * scale(s).
ossia::transform3d makeTransform(float tx, float s)
{
  ossia::transform3d t;
  const float m[16] = {0, s, 0, 0, -s, 0, 0, 0, 0, 0, s, 0, tx, 0, 0, 1};
  std::copy_n(m, 16, t.matrix);
  return t;
}
}

TEST_CASE("Merge Geometries applies each input's transform", "[gfx][merge][fixF]")
{
  alignas(64) static unsigned char rl_storage[16384]{};
  alignas(64) static unsigned char misc_storage[4096]{};
  auto& rl = *reinterpret_cast<RenderList*>(rl_storage);
  auto& batch = *reinterpret_cast<QRhiResourceUpdateBatch*>(misc_storage);
  auto& cb = *reinterpret_cast<QRhiCommandBuffer*>(misc_storage);
  QRhiResourceUpdateBatch* batch_ptr = &batch;

  MergeGeometriesNode merge;
  std::unique_ptr<NodeRenderer> r{merge.createRenderer(rl)};
  REQUIRE(r);

  SinkNode sinkNode;
  SinkRenderer sink{sinkNode};
  sinkNode.renderedNodes[&rl] = &sink;
  Edge edge{merge.output[0], sinkNode.input[0], Process::CableType::ImmediateGlutton};

  const auto a = makeTriangle();
  const auto b = makeTriangle();
  const char keyA{}, keyB{};

  r->process(0, a, &keyA);
  r->process(0, makeTransform(2.f, 0.5f));
  r->process(1, b, &keyB);

  r->update(rl, batch, nullptr);
  r->runInitialPasses(rl, cb, batch_ptr, edge);

  const auto* out = sink.findGeometryByPort(0);
  REQUIRE(out);
  REQUIRE(out->meshes);
  REQUIRE(out->meshes->meshes.size() == 2);

  SECTION("transformed input")
  {
    const auto& g = out->meshes->meshes[0];
    const float* f = floats(g);
    // (1,0,0) -> scale 0.5 -> rotate 90 -> (0, 0.5, 0) -> translate -> (2, 0.5, 0)
    CHECK(f[3] == Approx(2.f));
    CHECK(f[4] == Approx(0.5f));
    CHECK(f[5] == Approx(0.f).margin(1e-6));
    // (0,1,0) -> (-0.5, 0, 0) + (2,0,0)
    CHECK(f[6] == Approx(1.5f));
    CHECK(f[7] == Approx(0.f).margin(1e-6));
    // Origin moves to the translation.
    CHECK(f[0] == Approx(2.f));
    // Normals stay unit length and follow the rotation (+Z stays +Z).
    for(int v = 0; v < 3; v++)
    {
      CHECK(f[9 + 3 * v] == Approx(0.f).margin(1e-6));
      CHECK(f[9 + 3 * v + 2] == Approx(1.f));
    }
    CHECK(g.bounds.min[0] == Approx(1.5f));
    CHECK(g.bounds.max[0] == Approx(2.f));
    CHECK(g.bounds.max[1] == Approx(0.5f));
  }

  SECTION("identity input shares its buffer")
  {
    const auto& g = out->meshes->meshes[1];
    CHECK(floats(g) == floats(b.meshes->meshes[0]));
    CHECK(floats(g)[3] == 1.f);
  }

  SECTION("upstream buffers are untouched")
  {
    const float* f = floats(a.meshes->meshes[0]);
    CHECK(f[0] == 0.f);
    CHECK(f[3] == 1.f);
    CHECK(f[4] == 0.f);
  }

  SECTION("a new transform re-bakes the output")
  {
    r->process(0, makeTransform(-1.f, 1.f));
    r->update(rl, batch, nullptr);
    r->runInitialPasses(rl, cb, batch_ptr, edge);
    const auto* out2 = sink.findGeometryByPort(0);
    REQUIRE(out2);
    const float* f = floats(out2->meshes->meshes[0]);
    CHECK(f[0] == Approx(-1.f));
    CHECK(f[3] == Approx(-1.f));
    CHECK(f[4] == Approx(1.f));
  }

  r->release(rl);
  sinkNode.renderedNodes.clear();
}
