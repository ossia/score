// "PRIMITIVE_DATA": true expands an indexed mesh into a non-indexed list. The
// upstream here re-pushes the same geometry every frame with its buffers left
// dirty, as a CPU producer does: NodeRenderer then reports a geometry change
// on every frame, and the expansion must still be reused instead of rebuilt
// and re-uploaded. The reload count comes from the SCORE_BUFTRACE channel
// ("CustomMesh::reload"), and the picture must stay the one the first frame
// drew. A producer that rewrites a buffer in place (same pointer, size and
// dirty_index, as avendish's CPU geometry outputs do) must still be expanded
// again.
#include <score_test/Gfx.hpp>

#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <QtGlobal>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <atomic>
#include <cstring>
#include <utility>

using namespace score::test::gfx;

namespace
{
QString corpus(const char* file)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR) + QStringLiteral("/") + file;
}

struct DirtyMeshNode final : score::gfx::ProcessNode
{
  DirtyMeshNode()
  {
    output.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Geometry, {}});
  }
  //! Set to rewrite the index buffer in place, triangles swapped, on the next
  //! frame.
  bool swapTriangles{};
  score::gfx::NodeRenderer* createRenderer(score::gfx::RenderList&) const
      noexcept override;
};

struct DirtyMeshRenderer final : score::gfx::NodeRenderer
{
  const DirtyMeshNode& node;
  ossia::geometry_spec m_spec;

  explicit DirtyMeshRenderer(const DirtyMeshNode& n)
      : NodeRenderer{n}
      , node{n}
  {
  }

  void init(score::gfx::RenderList&, QRhiResourceUpdateBatch&) override
  {
    // Two triangles sharing vertex 1 (indices 0 1 2, 3 4 1), symmetric about
    // y = 0: same fixture as GfxRawRasterPrimitiveData.cpp.
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
    auto& g = m_spec.meshes->meshes[0];
    if(std::exchange(const_cast<DirtyMeshNode&>(node).swapTriangles, false))
    {
      static const uint32_t swapped[6] = {3, 4, 1, 0, 1, 2};
      auto& cpu = *ossia::get_if<ossia::geometry::cpu_buffer>(&g.buffers[1].data);
      std::memcpy(cpu.raw_data.get(), swapped, sizeof(swapped));
    }
    // The consumer never runs acquireMesh on this spec, so nothing clears
    // these: keep them set as a producer re-publishing every frame would.
    for(auto& b : g.buffers)
      b.dirty = true;
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
DirtyMeshNode::createRenderer(score::gfx::RenderList&) const noexcept
{
  return new DirtyMeshRenderer{*this};
}

std::atomic_int g_reloads{0};
QtMessageHandler g_previousHandler{};

void countReloads(QtMsgType type, const QMessageLogContext& ctx, const QString& msg)
{
  if(msg.contains(QLatin1String("CustomMesh::reload")))
  {
    g_reloads.fetch_add(1, std::memory_order_relaxed);
    return;
  }
  if(g_previousHandler)
    g_previousHandler(type, ctx, msg);
}

constexpr int kSize = 64;
}

TEST_CASE(
    "PRIMITIVE_DATA reuses its expansion while the source geometry is unchanged",
    "[gfx][raster][primitive]")
{
  // buftrace_enabled() caches the variable on first use.
  qputenv("SCORE_BUFTRACE", "1");

  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  bool skipped = false;
  std::string err;
  int steadyReloads = -1;
  ReadbackImage first, last;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int producer = p.addNode(std::make_unique<DirtyMeshNode>());
    const int raster = p.addRaster(
        corpus("rr-primitive-data-mesh.vs"), corpus("rr-primitive-data-mesh.fs"));
    if(producer < 0 || raster < 0)
    {
      err = p.error();
      return;
    }
    p.wire(p.nodeGeometryOut(producer, 0), p.geometryIn(raster, 0));
    const int sink = p.addSink({kSize, kSize});
    p.wire(p.imageOut(raster, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      skipped = p.skipped();
      err = skipped ? std::string{} : p.error();
      return;
    }
    p.render(3);
    first = p.readback(sink);

    g_reloads = 0;
    g_previousHandler = qInstallMessageHandler(countReloads);
    p.render(8);
    qInstallMessageHandler(g_previousHandler);
    g_previousHandler = {};
    steadyReloads = g_reloads.load();

    last = p.readback(sink);
    if(!first.valid() || !last.valid())
      err = "empty readback";
  });
  if(skipped)
    SKIP("backend unavailable");

  INFO("error=" << err);
  REQUIRE(err.empty());

  CHECK(steadyReloads == 0);

  const int row = kSize / 2;
  for(int x : {6, kSize - 7})
  {
    const auto a = first.at(x, row);
    const auto b = last.at(x, row);
    INFO("x=" << x);
    CHECK(int(a[0]) == int(b[0]));
    CHECK(int(a[1]) == int(b[1]));
    CHECK(int(a[2]) == int(b[2]));
  }
  // The right triangle is primitive 1: red 128 (see GfxRawRasterPrimitiveData).
  CHECK(std::abs(int(last.at(kSize - 7, row)[0]) - 128) <= 2);
}

TEST_CASE(
    "PRIMITIVE_DATA expands again when a source buffer is rewritten in place",
    "[gfx][raster][primitive]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  bool skipped = false;
  std::string err;
  ReadbackImage before, after;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    auto node = std::make_unique<DirtyMeshNode>();
    auto* mesh = node.get();
    const int producer = p.addNode(std::move(node));
    const int raster = p.addRaster(
        corpus("rr-primitive-data-mesh.vs"), corpus("rr-primitive-data-mesh.fs"));
    if(producer < 0 || raster < 0)
    {
      err = p.error();
      return;
    }
    p.wire(p.nodeGeometryOut(producer, 0), p.geometryIn(raster, 0));
    const int sink = p.addSink({kSize, kSize});
    p.wire(p.imageOut(raster, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      skipped = p.skipped();
      err = skipped ? std::string{} : p.error();
      return;
    }
    p.render(3);
    before = p.readback(sink);
    mesh->swapTriangles = true;
    p.render(3);
    after = p.readback(sink);
    if(!before.valid() || !after.valid())
      err = "empty readback";
  });
  if(skipped)
    SKIP("backend unavailable");

  INFO("error=" << err);
  REQUIRE(err.empty());

  // Red = PRIMITIVE_ID / 2: the right triangle goes from primitive 1 to 0 and
  // the left one from 0 to 1.
  const int row = kSize / 2;
  CHECK(std::abs(int(before.at(kSize - 7, row)[0]) - 128) <= 2);
  CHECK(int(before.at(6, row)[0]) <= 2);
  CHECK(int(after.at(kSize - 7, row)[0]) <= 2);
  CHECK(std::abs(int(after.at(6, row)[0]) - 128) <= 2);
}
