// "PRIMITIVE_DATA": true gives a raw raster fragment stage PRIMITIVE_ID and
// BARYCENTRIC (agent A5).
//
// Both come from gl_VertexIndex in the vertex stage (primitive = index / 3,
// corner = index % 3 for a triangle list), so they need every primitive to
// own its vertices. A procedural draw has that already; an indexed mesh is
// expanded into a list by the renderer. The fixtures draw two triangles
// symmetric about y = 0, so the middle row reads the same whichever way up
// the backend lands the picture: on it, the left triangle has barycentric
// weights ((1 - b1) / 2, x + 1, (1 - b1) / 2) and the right one
// (x / 2, x / 2, 1 - x).
//
// The mesh case shares a vertex between the two triangles (indices 0 1 2,
// 3 4 1): drawn indexed, the right triangle's corners would read 0, 1, 1.
//
// Registration:
//   score_add_gfx_test(raw_raster_primitive_data_a5 GfxRawRasterPrimitiveDataA5.cpp)
#include <score_test/Gfx.hpp>

#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <cstring>

using namespace score::test::gfx;

namespace
{
QString corpus(const char* file)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR) + QStringLiteral("/") + file;
}

struct IndexedMeshNode final : score::gfx::ProcessNode
{
  IndexedMeshNode()
  {
    output.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Geometry, {}});
  }
  score::gfx::NodeRenderer* createRenderer(score::gfx::RenderList&) const
      noexcept override;
};

struct IndexedMeshRenderer final : score::gfx::NodeRenderer
{
  const IndexedMeshNode& node;
  ossia::geometry_spec m_spec;

  explicit IndexedMeshRenderer(const IndexedMeshNode& n)
      : NodeRenderer{n}
      , node{n}
  {
  }

  void init(score::gfx::RenderList&, QRhiResourceUpdateBatch&) override
  {
    static const float pos[20]
        = {-1.f, -1.f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f, -1.f, 1.f,
           0.f,  1.f,  1.f, -1.f, 0.f, 1.f, 1.f, 1.f, 0.f, 1.f};
    static const uint32_t idx[6] = {0, 1, 2, 3, 4, 1};
    auto vbuf = std::shared_ptr<char[]>(new char[sizeof(pos)]);
    std::memcpy(vbuf.get(), pos, sizeof(pos));
    auto ibuf = std::shared_ptr<char[]>(new char[sizeof(idx)]);
    std::memcpy(ibuf.get(), idx, sizeof(idx));

    ossia::geometry g;
    g.vertices = 5;
    g.indices = 6;
    g.topology = ossia::geometry::triangles;
    g.cull_mode = ossia::geometry::none;
    g.front_face = ossia::geometry::counter_clockwise;
    g.buffers.push_back(
        {.data = ossia::geometry::cpu_buffer{
             std::shared_ptr<void>(vbuf, vbuf.get()), (int64_t)sizeof(pos)},
         .dirty = true});
    g.buffers.push_back(
        {.data = ossia::geometry::cpu_buffer{
             std::shared_ptr<void>(ibuf, ibuf.get()), (int64_t)sizeof(idx)},
         .dirty = true});
    g.bindings.push_back({.byte_stride = 16});
    ossia::geometry::attribute a_pos;
    a_pos.binding = 0;
    a_pos.location = 0;
    a_pos.format = ossia::geometry::attribute::float4;
    a_pos.semantic = ossia::attribute_semantic::position;
    g.attributes.push_back(a_pos);
    g.input.push_back({.buffer = 0, .byte_offset = 0});
    g.index.buffer = 1;
    g.index.format = decltype(g.index)::uint32;

    m_spec.meshes = std::make_shared<ossia::mesh_list>();
    m_spec.meshes->meshes.push_back(std::move(g));
    m_spec.filters = std::make_shared<ossia::geometry_filter_list>();
    m_initialized = true;
  }

  void update(score::gfx::RenderList&, QRhiResourceUpdateBatch&, score::gfx::Edge*)
      override
  {
  }

  void runInitialPasses(
      score::gfx::RenderList& renderer, QRhiCommandBuffer&, QRhiResourceUpdateBatch*&,
      score::gfx::Edge& edge) override
  {
    auto* sink = edge.sink;
    if(edge.source != node.output[0] || !m_spec.meshes || !sink || !sink->node)
      return;
    auto rn_it = sink->node->renderedNodes.find(&renderer);
    if(rn_it == sink->node->renderedNodes.end())
      return;
    auto it = std::find(sink->node->input.begin(), sink->node->input.end(), sink);
    if(it == sink->node->input.end())
      return;
    rn_it->second->process(int(it - sink->node->input.begin()), m_spec, edge.source);
  }

  void runRenderPass(score::gfx::RenderList&, QRhiCommandBuffer&, score::gfx::Edge&)
      override
  {
  }
  void removeOutputPass(score::gfx::RenderList&, score::gfx::Edge&) override { }
  void release(score::gfx::RenderList&) override
  {
    m_spec = {};
    m_initialized = false;
  }
};

score::gfx::NodeRenderer*
IndexedMeshNode::createRenderer(score::gfx::RenderList&) const noexcept
{
  return new IndexedMeshRenderer{*this};
}

constexpr int kSize = 64;

int expected(double v)
{
  return int(v * 255.0 + 0.5);
}
}

TEST_CASE("PRIMITIVE_DATA gives the fragment stage its primitive and barycentrics", "[gfx][raster][primitive]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const bool mesh = GENERATE(false, true);
  CAPTURE(backend_name(api), mesh);

  bool skipped = false;
  std::string err;
  ReadbackImage img;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    int raster = -1;
    if(mesh)
    {
      const int producer = p.addNode(std::make_unique<IndexedMeshNode>());
      raster = p.addRaster(
          corpus("rr-a5-primitive-data-mesh.vs"), corpus("rr-a5-primitive-data-mesh.fs"));
      if(producer < 0 || raster < 0)
      {
        err = p.error();
        return;
      }
      p.wire(p.nodeGeometryOut(producer, 0), p.geometryIn(raster, 0));
    }
    else
    {
      raster = p.addRaster(
          corpus("rr-a5-primitive-data.vs"), corpus("rr-a5-primitive-data.fs"));
      if(raster < 0)
      {
        err = p.error();
        return;
      }
    }
    const int sink = p.addSink({kSize, kSize});
    p.wire(p.imageOut(raster, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      skipped = p.skipped();
      err = skipped ? std::string{} : p.error();
      return;
    }
    p.render(3);
    img = p.readback(sink);
    if(!img.valid())
      err = "empty readback";
  });
  if(skipped)
    SKIP("backend unavailable");

  INFO("error=" << err);
  REQUIRE(err.empty());

  const int row = kSize / 2;
  const int left = 6;
  const int right = kSize - 1 - left;
  const double x = (right + 0.5) / kSize * 2.0 - 1.0;
  const double b1Left = 1.0 - x;

  const auto l = img.at(left, row);
  const auto r = img.at(right, row);
  INFO("left " << int(l[0]) << " " << int(l[1]) << " " << int(l[2]));
  INFO("right " << int(r[0]) << " " << int(r[1]) << " " << int(r[2]));

  CHECK(int(l[0]) <= 2);
  CHECK(std::abs(int(l[1]) - expected((1.0 - b1Left) / 2.0)) <= 5);
  CHECK(std::abs(int(l[2]) - expected(b1Left)) <= 5);

  CHECK(std::abs(int(r[0]) - 128) <= 2);
  CHECK(std::abs(int(r[1]) - expected(x / 2.0)) <= 5);
  CHECK(std::abs(int(r[2]) - expected(x / 2.0)) <= 5);
}
