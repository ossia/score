// Triangle fans on backends without them. D3D11, D3D12 and Metal report no
// QRhi::TriangleFanTopology: given a TriangleFan pipeline, the D3D11 and Metal
// backends reach Q_UNREACHABLE and D3D12 draws a strip. Fans come from VSA
// PRIMITIVE_MODE TRI_FAN, raw PIPELINE_STATE TOPOLOGY triangle_fan, and
// geometry labelled triangle_fan (BufferToGeometry, CSF TOPOLOGY).
//
// SCORE_GFX_NO_TRIANGLE_FAN=1 makes the engine treat the backend as one of
// those, so the fallbacks run on Vulkan / OpenGL here. Each case draws a
// regular hexagon of radius 0.8 as a fan of 8 vertices (centre, 7 rim points)
// and must cover the hexagon both natively and through the fallback. The
// "RHI topology caps" line is the positive control that the switch reached
// the renderer, without which both runs would take the native path.
//
//   * a CPU fan geometry, plain and indexed (uint16, rim order reversed), into
//     a raw raster in Mode Geometry: lowered to an indexed triangle list;
//   * VSA TRI_FAN: drawn as Triangles from an index list over the ids.
//
// A fan the engine cannot rewrite (a procedural draw: PIPELINE_STATE TOPOLOGY
// triangle_fan with VERTEX_COUNT and no vertex data; or a GPU index buffer /
// indirect draw) is not drawn there, with a warning: it was drawn as a
// triangle list, other triangles than the fan's.
#include <score_test/Gfx.hpp>

#include "GfxLogCapture.hpp"

#include <Gfx/Graph/GeometryTopology.hpp>
#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <cmath>
#include <cstring>
#include <string>
#include <vector>

using namespace score::test::gfx;

namespace
{
constexpr int ModeGeometry = 3;
// (3 sqrt(3) / 2) r^2 over the 2 x 2 frame, r = 0.8.
const double hexagonFraction = 3. * std::sqrt(3.) / 2. * 0.64 / 4.;

QString corpus(const char* file)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR) + QStringLiteral("/") + file;
}

struct FanMeshNode final : score::gfx::ProcessNode
{
  explicit FanMeshNode(bool indexed)
      : indexed{indexed}
  {
    output.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Geometry, {}});
  }
  bool indexed{};
  score::gfx::NodeRenderer* createRenderer(score::gfx::RenderList&) const
      noexcept override;
};

ossia::geometry hexagonFan(bool indexed)
{
  constexpr int N = 8;
  std::vector<float> vtx;
  for(int k = 0; k < N; k++)
  {
    float x = 0.f, y = 0.f;
    if(k > 0)
    {
      const float a = float(k - 1) * 6.28318530718f / 6.f;
      x = 0.8f * std::cos(a);
      y = 0.8f * std::sin(a);
    }
    vtx.insert(vtx.end(), {x, y, 0.f, 1.f, 1.f, 1.f, 1.f, 1.f});
  }
  const auto vbytes = vtx.size() * sizeof(float);
  auto vbuf = std::shared_ptr<char[]>(new char[vbytes]);
  std::memcpy(vbuf.get(), vtx.data(), vbytes);

  ossia::geometry g;
  g.vertices = N;
  g.topology = ossia::geometry::triangle_fan;
  g.cull_mode = ossia::geometry::none;
  g.front_face = ossia::geometry::counter_clockwise;
  g.buffers.push_back(
      {.data = ossia::geometry::cpu_buffer{
           std::shared_ptr<void>(vbuf, vbuf.get()), (int64_t)vbytes},
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

  if(indexed)
  {
    // Centre first, then the rim backwards: the same hexagon, wound clockwise.
    const uint16_t idx[N] = {0, 7, 6, 5, 4, 3, 2, 1};
    auto ibuf = std::shared_ptr<char[]>(new char[sizeof(idx)]);
    std::memcpy(ibuf.get(), idx, sizeof(idx));
    g.buffers.push_back(
        {.data = ossia::geometry::cpu_buffer{
             std::shared_ptr<void>(ibuf, ibuf.get()), (int64_t)sizeof(idx)},
         .dirty = true});
    g.index.buffer = 1;
    g.index.format = decltype(g.index)::uint16;
    g.indices = N;
  }
  return g;
}

struct FanMeshRenderer final : score::gfx::NodeRenderer
{
  const FanMeshNode& node;
  ossia::geometry_spec m_spec;

  explicit FanMeshRenderer(const FanMeshNode& n)
      : NodeRenderer{n}
      , node{n}
  {
  }

  void init(score::gfx::RenderList&, QRhiResourceUpdateBatch&) override
  {
    m_spec.meshes = std::make_shared<ossia::mesh_list>();
    m_spec.meshes->meshes.push_back(hexagonFan(node.indexed));
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
FanMeshNode::createRenderer(score::gfx::RenderList&) const noexcept
{
  return new FanMeshRenderer{*this};
}

enum class Source
{
  Mesh,
  IndexedMesh,
  Vsa,
  ProceduralFan
};

struct Outcome
{
  bool skipped{};
  std::string error;
  double litFraction{};
  int capsTriangleFan{-1};
  int notDrawnWarnings{};
};

struct NoFanSwitch
{
  explicit NoFanSwitch(bool on)
  {
    if(on)
      qputenv("SCORE_GFX_NO_TRIANGLE_FAN", "1");
    else
      qunsetenv("SCORE_GFX_NO_TRIANGLE_FAN");
  }
  ~NoFanSwitch() { qunsetenv("SCORE_GFX_NO_TRIANGLE_FAN"); }
};

Outcome run(score::gfx::GraphicsApi api, Source src, bool noFan)
{
  Outcome out;
  NoFanSwitch sw{noFan};
  std::vector<LogCapture::Message> messages;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    LogCapture log;
    GfxPipeline p;
    int image = -1;
    if(src == Source::Vsa)
    {
      image = p.addVsa(corpus("vsa-tri-fan.vs"));
      if(image < 0)
      {
        out.error = p.error();
        return;
      }
    }
    else if(src == Source::ProceduralFan)
    {
      image = p.addRaster(corpus("rr-procedural-fan.vs"), corpus("rr-procedural-fan.fs"));
      if(image < 0)
      {
        out.error = p.error();
        return;
      }
    }
    else
    {
      const int producer
          = p.addNode(std::make_unique<FanMeshNode>(src == Source::IndexedMesh));
      image = p.addRaster(corpus("raw-raster-basic.vs"), corpus("raw-raster-basic.fs"));
      if(producer < 0 || image < 0)
      {
        out.error = p.error();
        return;
      }
      p.wire(p.nodeGeometryOut(producer, 0), p.geometryIn(image, 0));
    }
    const int sink = p.addSink({64, 64});
    p.wire(p.imageOut(image, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      out.skipped = p.skipped();
      out.error = out.skipped ? std::string{} : p.error();
      return;
    }
    if(src == Source::Mesh || src == Source::IndexedMesh)
      setControl(
          *p.isf(image), nth_control_input(*p.isf(image), 0), ossia::value{ModeGeometry});
    p.render(3);
    messages = log.messages();
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

  for(const auto& m : messages)
  {
    if(auto i = m.text.indexOf(QStringLiteral("RHI topology caps: triangleFan="));
       i >= 0)
      out.capsTriangleFan = m.text.mid(i + 31, 1).toInt();
    if(m.text.contains(QStringLiteral("triangle fan")) && m.text.contains(QStringLiteral("not drawn")))
      out.notDrawnWarnings++;
  }
  return out;
}

const char* sourceName(Source s)
{
  switch(s)
  {
    case Source::Mesh:
      return "cpu-fan";
    case Source::IndexedMesh:
      return "cpu-fan-indexed";
    case Source::Vsa:
      return "vsa-fan";
    case Source::ProceduralFan:
      return "procedural-fan";
  }
  return "?";
}
}

TEST_CASE("lowerTriangleFan rewrites a fan as the same triangles", "[gfx][topology]")
{
  {
    auto g = hexagonFan(false);
    REQUIRE(score::gfx::lowerTriangleFan(g));
    CHECK(g.topology == ossia::geometry::triangles);
    CHECK(g.indices == 18);
    REQUIRE(g.index.buffer == 1);
    CHECK(g.index.format == decltype(g.index)::uint32);
    const auto& cpu = ossia::get<ossia::geometry::cpu_buffer>(g.buffers[1].data);
    const auto* idx = static_cast<const uint32_t*>(cpu.raw_data.get());
    CHECK(std::vector<uint32_t>(idx, idx + 6) == std::vector<uint32_t>{0, 1, 2, 0, 2, 3});
    CHECK(std::vector<uint32_t>(idx + 15, idx + 18) == std::vector<uint32_t>{0, 6, 7});
  }
  {
    auto g = hexagonFan(true);
    REQUIRE(score::gfx::lowerTriangleFan(g));
    CHECK(g.indices == 18);
    REQUIRE(g.index.buffer == 2);
    const auto& cpu = ossia::get<ossia::geometry::cpu_buffer>(g.buffers[2].data);
    const auto* idx = static_cast<const uint32_t*>(cpu.raw_data.get());
    CHECK(std::vector<uint32_t>(idx, idx + 6) == std::vector<uint32_t>{0, 7, 6, 0, 6, 5});
  }
  {
    // GPU index buffer: the order is not visible, nothing changes.
    auto g = hexagonFan(true);
    g.buffers[1].data = ossia::geometry::gpu_buffer{};
    CHECK(!score::gfx::lowerTriangleFan(g));
    CHECK(g.topology == ossia::geometry::triangle_fan);
  }
}

TEST_CASE(
    "triangle fans draw the same on a backend without them", "[gfx][topology][B06]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const auto src = GENERATE(Source::Mesh, Source::IndexedMesh, Source::Vsa);
  CAPTURE(backend_name(api), sourceName(src));

  const auto native = run(api, src, false);
  if(native.skipped)
    SKIP(backend_name(api) << ": no usable backend");
  INFO(native.error);
  REQUIRE(native.error.empty());
  CHECK(native.litFraction == Catch::Approx(hexagonFraction).margin(0.03));

  const auto fallback = run(api, src, true);
  INFO(fallback.error);
  REQUIRE(fallback.error.empty());
  CHECK(fallback.capsTriangleFan == 0);
  CHECK(fallback.litFraction == Catch::Approx(hexagonFraction).margin(0.03));
  CHECK(fallback.litFraction == Catch::Approx(native.litFraction).margin(0.005));
}

TEST_CASE(
    "a triangle fan that cannot be rewritten is not drawn on a backend without them",
    "[gfx][topology][B06]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  const auto native = run(api, Source::ProceduralFan, false);
  if(native.skipped)
    SKIP(backend_name(api) << ": no usable backend");
  INFO(native.error);
  REQUIRE(native.error.empty());
  // Metal (and D3D) have no fans: there the "native" run is already the
  // fallback, checked below.
  if(native.capsTriangleFan != 0)
  {
    CHECK(native.litFraction == Catch::Approx(hexagonFraction).margin(0.03));
    CHECK(native.notDrawnWarnings == 0);
  }

  const auto fallback = run(api, Source::ProceduralFan, true);
  INFO(fallback.error);
  REQUIRE(fallback.error.empty());
  CHECK(fallback.capsTriangleFan == 0);
  INFO("lit " << fallback.litFraction);
  CHECK(fallback.litFraction == 0.);
  CHECK(fallback.notDrawnWarnings >= 1); // once per pipeline the node builds
}
