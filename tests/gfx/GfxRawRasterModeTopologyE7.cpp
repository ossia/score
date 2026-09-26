// A raw raster whose shader declares no TOPOLOGY draws with its Mode control
// (Triangles, Points or Lines), whatever topology the cabled geometry has. A
// saved Mode of Points or Lines on a triangle mesh draws its corners or loose
// segments, which an impostor shader discards entirely: the engine says so
// once, with the value to set.
//
// Two producers of the same triangle (19.3 % of the frame) into
// raw-raster-basic.fs:
//   * a CPU mesh labelled triangles: Mode Triangles draws it silently, Mode
//     Points warns exactly once, across frames and a Points -> Triangles ->
//     Points round trip that rebuilds the pipeline twice;
//   * syn-geo-asym-tri.cs: a compute shader labels its geometry points
//     whatever it holds, and Mode is how its consumer says what to draw, so
//     neither Mode warns.
//
// Registration:
//   score_add_gfx_test(raw_raster_mode_topology_e7 GfxRawRasterModeTopologyE7.cpp)
#include <score_test/Gfx.hpp>

#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <QMutex>
#include <QStringList>

#include <algorithm>
#include <cstring>
#include <string>

using namespace score::test::gfx;

namespace
{
QString corpus(const char* file)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR) + QStringLiteral("/") + file;
}

QMutex g_logMutex;
QStringList g_log;
QtMessageHandler g_prevHandler{};

void captureHandler(QtMsgType t, const QMessageLogContext& ctx, const QString& msg)
{
  {
    QMutexLocker lock{&g_logMutex};
    g_log.push_back(msg);
  }
  if(g_prevHandler)
    g_prevHandler(t, ctx, msg);
}

struct TriangleMeshNode final : score::gfx::ProcessNode
{
  TriangleMeshNode()
  {
    output.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Geometry, {}});
  }
  score::gfx::NodeRenderer* createRenderer(score::gfx::RenderList&) const
      noexcept override;
};

struct TriangleMeshRenderer final : score::gfx::NodeRenderer
{
  const TriangleMeshNode& node;
  ossia::geometry_spec m_spec;

  explicit TriangleMeshRenderer(const TriangleMeshNode& n)
      : NodeRenderer{n}
      , node{n}
  {
  }

  void init(score::gfx::RenderList&, QRhiResourceUpdateBatch&) override
  {
    static const float vtx[24] = {
        -0.80f, -0.60f, 0.f, 1.f, 1.f, 0.f, 0.f, 1.f,
        0.55f,  -0.20f, 0.f, 1.f, 0.f, 1.f, 0.f, 1.f,
        -0.10f, 0.75f,  0.f, 1.f, 0.f, 0.f, 1.f, 1.f};
    auto vbuf = std::shared_ptr<char[]>(new char[sizeof(vtx)]);
    std::memcpy(vbuf.get(), vtx, sizeof(vtx));

    ossia::geometry g;
    g.vertices = 3;
    g.topology = ossia::geometry::triangles;
    g.cull_mode = ossia::geometry::none;
    g.front_face = ossia::geometry::counter_clockwise;
    g.buffers.push_back(
        {.data = ossia::geometry::cpu_buffer{
             std::shared_ptr<void>(vbuf, vbuf.get()), (int64_t)sizeof(vtx)},
         .dirty = true});
    g.bindings.push_back({.byte_stride = 32});
    ossia::geometry::attribute a_pos;
    a_pos.format = ossia::geometry::attribute::float4;
    a_pos.semantic = ossia::attribute_semantic::position;
    g.attributes.push_back(a_pos);
    ossia::geometry::attribute a_col;
    a_col.location = 1;
    a_col.format = ossia::geometry::attribute::float4;
    a_col.byte_offset = 16;
    a_col.semantic = ossia::attribute_semantic::color0;
    g.attributes.push_back(a_col);
    g.input.push_back({.buffer = 0, .byte_offset = 0});

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
TriangleMeshNode::createRenderer(score::gfx::RenderList&) const noexcept
{
  return new TriangleMeshRenderer{*this};
}

struct Outcome
{
  bool skipped{};
  std::string error;
  double litFraction{};
  int modeWarnings{};
  QString firstWarning;
};

Outcome run(score::gfx::GraphicsApi api, bool fromCompute, int mode)
{
  Outcome out;
  {
    QMutexLocker lock{&g_logMutex};
    g_log.clear();
  }
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    g_prevHandler = qInstallMessageHandler(captureHandler);
    struct Restore
    {
      ~Restore() { qInstallMessageHandler(g_prevHandler); }
    } restore;

    GfxPipeline p;
    const int producer = fromCompute ? p.addCsf(corpus("syn-geo-asym-tri.cs"))
                                     : p.addNode(std::make_unique<TriangleMeshNode>());
    const int raster
        = p.addRaster(corpus("raw-raster-basic.vs"), corpus("raw-raster-basic.fs"));
    if(producer < 0 || raster < 0)
    {
      out.error = p.error();
      return;
    }
    p.wire(
        fromCompute ? p.geometryOut(producer, 0) : p.nodeGeometryOut(producer, 0),
        p.geometryIn(raster, 0));
    const int sink = p.addSink({64, 64});
    p.wire(p.imageOut(raster, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      out.skipped = p.skipped();
      out.error = out.skipped ? std::string{} : p.error();
      return;
    }
    const int modePort = nth_control_input(*p.isf(raster), 0);
    setControl(*p.isf(raster), modePort, ossia::value{mode});
    p.render(3);
    if(mode != 0)
    {
      setControl(*p.isf(raster), modePort, ossia::value{0});
      p.render(3);
      setControl(*p.isf(raster), modePort, ossia::value{mode});
    }
    p.render(3);
    const auto img = p.readback(sink);
    if(!img.valid())
    {
      out.error = "empty readback";
      return;
    }
    int lit = 0;
    for(int y = 0; y < img.height; ++y)
      for(int x = 0; x < img.width; ++x)
      {
        const auto px = img.at(x, y);
        if(px[0] + px[1] + px[2] > 60)
          ++lit;
      }
    out.litFraction = double(lit) / double(img.width * img.height);
  });

  QMutexLocker lock{&g_logMutex};
  for(const QString& m : g_log)
    if(m.contains(QStringLiteral("the Mode control draws")))
    {
      if(out.modeWarnings++ == 0)
        out.firstWarning = m;
    }
  return out;
}
}

TEST_CASE(
    "a raw raster Mode that matches a triangle mesh draws it and warns nothing",
    "[gfx][raster][topology][e7]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  const Outcome o = run(api, false, 0);
  if(o.skipped)
    SKIP("backend unavailable");
  INFO("error=" << o.error << " lit=" << o.litFraction);
  REQUIRE(o.error.empty());
  CHECK(o.litFraction > 0.15);
  CHECK(o.litFraction < 0.25);
  CHECK(o.modeWarnings == 0);
}

TEST_CASE(
    "a raw raster Mode of Points on a triangle mesh warns once",
    "[gfx][raster][topology][e7]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  const Outcome o = run(api, false, 1);
  if(o.skipped)
    SKIP("backend unavailable");
  INFO("error=" << o.error << " lit=" << o.litFraction << " warning="
                << o.firstWarning.toStdString());
  REQUIRE(o.error.empty());
  CHECK(o.litFraction < 0.02);
  CHECK(o.modeWarnings == 1);
  CHECK(o.firstWarning.contains(QStringLiteral(
      "the Mode control draws Points but the cabled geometry is Triangles; set Mode "
      "to Triangles")));
}

TEST_CASE(
    "a raw raster Mode on compute-shader geometry warns nothing",
    "[gfx][raster][topology][e7]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const int mode = GENERATE(0, 1);
  CAPTURE(backend_name(api), mode);
  const Outcome o = run(api, true, mode);
  if(o.skipped)
    SKIP("backend unavailable");
  if(const char* why = compute_shader_skip_reason(api))
    SKIP(why);
  INFO("error=" << o.error << " lit=" << o.litFraction);
  REQUIRE(o.error.empty());
  if(mode == 0)
    CHECK(o.litFraction > 0.15);
  else
    CHECK(o.litFraction < 0.02);
  CHECK(o.modeWarnings == 0);
}
