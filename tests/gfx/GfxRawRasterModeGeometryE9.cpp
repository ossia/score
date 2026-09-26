// The raw raster Mode control's fourth value, Geometry, draws with the cabled
// geometry's own topology instead of replacing it, and never warns. A compute
// shader's output geometry is labelled points unless its RESOURCES entry
// declares TOPOLOGY, which also replaces what a filter inherits upstream.
//
// The same lopsided triangle (19.3 % of the frame) into raw-raster-basic.fs:
//   * a CPU mesh labelled triangles or points: Geometry draws the triangle or
//     its three corners;
//   * syn-geo-asym-tri.cs (no TOPOLOGY): Geometry draws points;
//   * e9-geo-asym-tri-triangles.cs (TOPOLOGY triangles): Geometry draws the
//     triangle, and Points on it now warns like on a CPU triangle mesh;
//   * syn-geo-asym-tri.cs -> e9-filter-topology-triangles.cs: the filter's
//     declaration beats the points it inherits, on every frame the filter
//     refreshes its output in place, not only the one that builds it.
// libisf parses TOPOLOGY, writes it back and rejects an unknown value.
//
// Registration:
//   score_add_gfx_test(raw_raster_mode_geometry_e9 GfxRawRasterModeGeometryE9.cpp)
#include <score_test/Gfx.hpp>

#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <QFile>
#include <QMutex>
#include <QStringList>

#include <isf.hpp>

#include <algorithm>
#include <cstring>
#include <string>

using namespace score::test::gfx;

namespace
{
constexpr int ModeTriangles = 0;
constexpr int ModePoints = 1;
constexpr int ModeGeometry = 3;

QString corpus(const char* file)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR) + QStringLiteral("/") + file;
}

std::string readCorpus(const char* file)
{
  QFile f{corpus(file)};
  if(!f.open(QIODevice::ReadOnly))
    return {};
  return f.readAll().toStdString();
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

struct LabelledMeshNode final : score::gfx::ProcessNode
{
  explicit LabelledMeshNode(decltype(ossia::geometry::topology) t)
      : topology{t}
  {
    output.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Geometry, {}});
  }
  decltype(ossia::geometry::topology) topology;
  score::gfx::NodeRenderer* createRenderer(score::gfx::RenderList&) const
      noexcept override;
};

struct LabelledMeshRenderer final : score::gfx::NodeRenderer
{
  const LabelledMeshNode& node;
  ossia::geometry_spec m_spec;

  explicit LabelledMeshRenderer(const LabelledMeshNode& n)
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
    g.topology = node.topology;
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
LabelledMeshNode::createRenderer(score::gfx::RenderList&) const noexcept
{
  return new LabelledMeshRenderer{*this};
}

struct Outcome
{
  bool skipped{};
  std::string error;
  double litFraction{};
  int modeWarnings{};
  QString firstWarning;
  int deliveredTopology{-1};
};

enum class Source
{
  MeshTriangles,
  MeshPoints,
  CsfUndeclared,
  CsfDeclared,
  CsfFilterDeclared,
};

// Renders with `mode`, round-trips through Triangles (two pipeline rebuilds
// that re-read the cabled geometry's topology), and reads the result back.
Outcome run(score::gfx::GraphicsApi api, Source src, int mode)
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
    int producer = -1;
    int filter = -1;
    switch(src)
    {
      case Source::MeshTriangles:
        producer = p.addNode(std::make_unique<LabelledMeshNode>(ossia::geometry::triangles));
        break;
      case Source::MeshPoints:
        producer = p.addNode(std::make_unique<LabelledMeshNode>(ossia::geometry::points));
        break;
      case Source::CsfUndeclared:
        producer = p.addCsf(corpus("syn-geo-asym-tri.cs"));
        break;
      case Source::CsfDeclared:
        producer = p.addCsf(corpus("e9-geo-asym-tri-triangles.cs"));
        break;
      case Source::CsfFilterDeclared:
        producer = p.addCsf(corpus("syn-geo-asym-tri.cs"));
        filter = p.addCsf(corpus("e9-filter-topology-triangles.cs"));
        break;
    }
    const int raster
        = p.addRaster(corpus("raw-raster-basic.vs"), corpus("raw-raster-basic.fs"));
    if(producer < 0 || raster < 0 || (src == Source::CsfFilterDeclared && filter < 0))
    {
      out.error = p.error();
      return;
    }
    const bool mesh = src == Source::MeshTriangles || src == Source::MeshPoints;
    if(filter >= 0)
    {
      p.wire(p.geometryOut(producer, 0), p.geometryIn(filter, 0));
      p.wire(p.geometryOut(filter, 0), p.geometryIn(raster, 0));
    }
    else
    {
      p.wire(
          mesh ? p.nodeGeometryOut(producer, 0) : p.geometryOut(producer, 0),
          p.geometryIn(raster, 0));
    }
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
    setControl(*p.isf(raster), modePort, ossia::value{ModeTriangles});
    p.render(3);
    setControl(*p.isf(raster), modePort, ossia::value{mode});
    p.render(3);
    const auto img = p.readback(sink);
    if(!img.valid())
    {
      out.error = "empty readback";
      return;
    }
    auto& rasterNode = *p.isf(raster);
    const auto geoPort = std::find(
        rasterNode.input.begin(), rasterNode.input.end(), p.geometryIn(raster, 0));
    if(geoPort != rasterNode.input.end() && !rasterNode.renderedNodes.empty())
    {
      const auto* spec = rasterNode.renderedNodes.begin()->second->findGeometryByPort(
          int(geoPort - rasterNode.input.begin()));
      if(spec && spec->meshes && !spec->meshes->meshes.empty())
        out.deliveredTopology = spec->meshes->meshes[0].topology;
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

bool isCsf(Source s)
{
  return s != Source::MeshTriangles && s != Source::MeshPoints;
}

const char* sourceName(Source s)
{
  switch(s)
  {
    case Source::MeshTriangles:
      return "mesh-triangles";
    case Source::MeshPoints:
      return "mesh-points";
    case Source::CsfUndeclared:
      return "csf-undeclared";
    case Source::CsfDeclared:
      return "csf-declared";
    case Source::CsfFilterDeclared:
      return "csf-filter-declared";
  }
  return "?";
}

std::string csfWithTopology(const std::string& topology)
{
  return R"(/*{
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "RESOURCES": [
    {
      "NAME": "geo",
      "TYPE": "geometry",
      "VERTEX_COUNT": "3",
      "TOPOLOGY": ")"
         + topology + R"(",
      "ATTRIBUTES": [
        { "NAME": "position", "SEMANTIC": "position", "TYPE": "vec4", "ACCESS": "write_only" }
      ]
    }
  ],
  "PASSES": [ { "LOCAL_SIZE": [3, 1, 1], "EXECUTION_MODEL": { "TYPE": "PER_VERTEX" } } ]
}*/
void main() { }
)";
}

const isf::geometry_input* firstGeometry(const isf::descriptor& d)
{
  for(const auto& in : d.inputs)
    if(auto* g = ossia::get_if<isf::geometry_input>(&in.data))
      return g;
  return nullptr;
}
}

TEST_CASE(
    "a raw raster in Mode Geometry draws with the cabled geometry's topology",
    "[gfx][raster][topology][e9]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const auto src = GENERATE(
      Source::MeshTriangles, Source::MeshPoints, Source::CsfUndeclared,
      Source::CsfDeclared, Source::CsfFilterDeclared);
  CAPTURE(backend_name(api), sourceName(src));
  const Outcome o = run(api, src, ModeGeometry);
  if(o.skipped)
    SKIP("backend unavailable");
  if(isCsf(src))
    if(const char* why = compute_shader_skip_reason(api))
      SKIP(why);
  INFO("error=" << o.error << " lit=" << o.litFraction << " topology="
                << o.deliveredTopology << " warning=" << o.firstWarning.toStdString());
  REQUIRE(o.error.empty());
  const bool drawsTriangle = src == Source::MeshTriangles || src == Source::CsfDeclared
                             || src == Source::CsfFilterDeclared;
  CHECK(
      o.deliveredTopology
      == int(drawsTriangle ? ossia::geometry::triangles : ossia::geometry::points));
  if(drawsTriangle)
  {
    CHECK(o.litFraction > 0.15);
    CHECK(o.litFraction < 0.25);
  }
  else
  {
    CHECK(o.litFraction < 0.02);
  }
  CHECK(o.modeWarnings == 0);
}

TEST_CASE(
    "a raw raster Mode of Points on a CSF declared triangles warns once",
    "[gfx][raster][topology][e9]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  const Outcome o = run(api, Source::CsfDeclared, ModePoints);
  if(o.skipped)
    SKIP("backend unavailable");
  if(const char* why = compute_shader_skip_reason(api))
    SKIP(why);
  INFO("error=" << o.error << " lit=" << o.litFraction);
  REQUIRE(o.error.empty());
  CHECK(o.litFraction < 0.02);
  CHECK(o.modeWarnings == 1);
  CHECK(o.firstWarning.contains(
      QStringLiteral("set Mode to Triangles or Geometry")));
}

TEST_CASE("the raw raster Mode control offers Geometry as value 3", "[gfx][raster][e9]")
{
  isf::parser p{
      readCorpus("raw-raster-basic.vs"), readCorpus("raw-raster-basic.fs"), 450,
      isf::parser::ShaderType::RawRasterPipeline};
  const auto& ins = p.data().inputs;
  const auto it = std::find_if(
      ins.begin(), ins.end(), [](const isf::input& i) { return i.name == "Mode"; });
  REQUIRE(it != ins.end());
  const auto* l = ossia::get_if<isf::long_input>(&it->data);
  REQUIRE(l);
  REQUIRE(l->labels.size() == 4);
  CHECK(l->labels[0] == "Triangles");
  CHECK(l->labels[1] == "Points");
  CHECK(l->labels[2] == "Lines");
  CHECK(l->labels[3] == "Geometry");
  CHECK(ossia::get<int64_t>(l->values[3]) == 3);
  CHECK(l->def == 0);
}

TEST_CASE("a CSF geometry TOPOLOGY is parsed, written back and validated", "[isf][e9]")
{
  {
    isf::parser p{csfWithTopology("Triangle_Strip"), isf::parser::ShaderType::CSF};
    const auto* g = firstGeometry(p.data());
    REQUIRE(g);
    CHECK(g->topology == "triangle_strip");
    CHECK(p.write_isf().find("\"TOPOLOGY\": \"triangle_strip\"") != std::string::npos);
  }
  {
    const std::string undeclared = [] {
      auto s = csfWithTopology("points");
      const auto pos = s.find("      \"TOPOLOGY\": \"points\",\n");
      s.erase(pos, std::string("      \"TOPOLOGY\": \"points\",\n").size());
      return s;
    }();
    isf::parser p{undeclared, isf::parser::ShaderType::CSF};
    const auto* g = firstGeometry(p.data());
    REQUIRE(g);
    CHECK(g->topology.empty());
    CHECK(p.write_isf().find("TOPOLOGY") == std::string::npos);
  }
  CHECK_THROWS_AS(
      (isf::parser{csfWithTopology("quads"), isf::parser::ShaderType::CSF}),
      isf::invalid_file);
}
