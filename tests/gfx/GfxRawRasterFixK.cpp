// Raw raster pipeline fixes, agent K.
//
// N70: a raster with a single colour OUTPUT that declares FORMAT renders into
// a texture of that format, not into the consumer's RGBA8 render target.
// N58: a required vertex input with no upstream attribute still skips the
// draw, but says so at warning level and names the input.
// N49: an OUTPUT declaring SAMPLES 1 renders single-sampled under a 4x
// renderer; one declaring nothing keeps the renderer's MSAA.
// R10: the strict vertex-input resolver refuses a geometry binding past the
// pipeline's mesh bindings instead of renumbering it onto slot 0.
#include <Gfx/Settings/Model.hpp>

#include <Gfx/Graph/Utils.hpp>
#include <Gfx/Graph/VertexFallbackPlan.hpp>

#include <score_test/Gfx.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <QMutex>
#include <QStringList>

using namespace score::test::gfx;

namespace
{
QString corpus(const char* file)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR) + QStringLiteral("/") + file;
}

QMutex g_warningsMutex;
QStringList g_warnings;
QtMessageHandler g_previousHandler{};

void captureWarnings(QtMsgType type, const QMessageLogContext& ctx, const QString& msg)
{
  if(type == QtWarningMsg)
  {
    QMutexLocker lock{&g_warningsMutex};
    g_warnings.push_back(msg);
  }
  if(g_previousHandler)
    g_previousHandler(type, ctx, msg);
}
}

TEST_CASE(
    "a single colour OUTPUT renders in its declared FORMAT",
    "[gfx][raster][format][fixK]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  bool skipped = false;
  std::string err;
  ReadbackImage img;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int raster = p.addRaster(
        corpus("rr-fixk-single-output-format.vs"),
        corpus("rr-fixk-single-output-format.fs"));
    if(raster < 0)
    {
      err = p.error();
      return;
    }
    const int sink = p.addSink({32, 32});
    p.wire(p.imageOut(raster, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      skipped = p.skipped();
      err = skipped ? std::string{} : p.error();
      return;
    }
    p.render(2);
    img = p.readback(sink);
    if(!img.valid())
      err = "empty readback";
  });
  if(skipped)
    SKIP("backend unavailable");

  INFO("error=" << err);
  REQUIRE(err.empty());
  // 256 additive layers of 1/1024: 0.25 in a float target, 0 in an 8-bit one.
  const auto px = img.center();
  INFO("centre = " << int(px[0]) << " " << int(px[1]) << " " << int(px[2]));
  CHECK(px[0] >= 56);
  CHECK(px[0] <= 72);
}

TEST_CASE(
    "a missing required vertex input is reported as a warning naming it",
    "[gfx][raster][vertex-input][fixK]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  if(const char* why = compute_shader_skip_reason(api))
    SKIP(why);

  {
    QMutexLocker lock{&g_warningsMutex};
    g_warnings.clear();
  }

  bool skipped = false;
  std::string err;
  ReadbackImage img;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int prod = p.addCsf(corpus("syn-geo-count-user.cs"));
    const int raster = p.addRaster(
        corpus("rr-fixk-missing-input.vs"), corpus("rr-fixk-missing-input.fs"));
    if(prod < 0 || raster < 0)
    {
      err = p.error();
      return;
    }
    const int sink = p.addSink({32, 32});
    p.wire(p.geometryOut(prod, 0), p.geometryIn(raster, 0));
    p.wire(p.imageOut(raster, 0), p.sinkInput(sink));
    g_previousHandler = qInstallMessageHandler(captureWarnings);
    const bool created = p.create(api);
    if(created)
      p.render(3);
    qInstallMessageHandler(g_previousHandler);
    g_previousHandler = nullptr;
    if(!created)
    {
      skipped = p.skipped();
      err = skipped ? std::string{} : p.error();
      return;
    }
    img = p.readback(sink);
  });
  if(skipped)
    SKIP("backend unavailable");

  INFO("error=" << err);
  REQUIRE(err.empty());

  QStringList named;
  {
    QMutexLocker lock{&g_warningsMutex};
    for(const auto& w : g_warnings)
      if(w.contains(QStringLiteral("fixk_absent")))
        named.push_back(w);
  }
  INFO("warnings naming fixk_absent: " << named.join(" | ").toStdString());
  CHECK(!named.isEmpty());

  // The draw is still skipped: nothing white reaches the sink.
  if(img.valid())
  {
    const auto px = img.center();
    INFO("centre = " << int(px[0]) << " " << int(px[1]) << " " << int(px[2]));
    CHECK(px[0] < 200);
  }
}

namespace
{
struct samples_guard
{
  Gfx::Settings::Model& gfx;
  int previous{gfx.getSamples()};
  ~samples_guard() { gfx.setSamples(previous); }
};

struct EdgeShot
{
  bool skipped = false;
  std::string err;
  int rendererSamples = 0;
  int partial = 0;
  int lit = 0;
};

EdgeShot render_edge(score::gfx::GraphicsApi api, const char* fs)
{
  EdgeShot r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext& ctx) {
    samples_guard guard{ctx.settings<Gfx::Settings::Model>()};
    guard.gfx.setSamples(4);

    GfxPipeline p;
    const int raster = p.addRaster(corpus("rr-fixk-edge.vs"), corpus(fs));
    if(raster < 0)
    {
      r.err = p.error();
      return;
    }
    // The offscreen sink's own target is single-sampled; an ISF's input
    // target is allocated at the renderer's sample count.
    const int view = p.addIsf(corpus("fixk-view.fs"));
    if(view < 0)
    {
      r.err = p.error();
      return;
    }
    const int sink = p.addSink({64, 64});
    p.wire(p.imageOut(raster, 0), p.imageIn(view, 0));
    p.wire(p.imageOut(view, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      r.skipped = p.skipped();
      r.err = r.skipped ? std::string{} : p.error();
      return;
    }
    if(auto rs = p.sink(sink)->renderState())
      r.rendererSamples = rs->samples;
    p.render(2);
    const auto img = p.readback(sink);
    if(!img.valid())
    {
      r.err = "empty readback";
      return;
    }
    for(int y = 0; y < img.height; ++y)
      for(int x = 0; x < img.width; ++x)
      {
        const int v = img.at(x, y)[0];
        if(v > 24 && v < 232)
          ++r.partial;
        if(v >= 232)
          ++r.lit;
      }
  });
  return r;
}
}

TEST_CASE(
    "a declared SAMPLES 1 renders single-sampled under a multisampled renderer",
    "[gfx][raster][msaa][fixK]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  const auto msaa = render_edge(api, "rr-fixk-edge.fs");
  if(msaa.skipped)
    SKIP("backend unavailable");
  INFO("error=" << msaa.err);
  REQUIRE(msaa.err.empty());
  if(msaa.rendererSamples < 2)
    SKIP("the renderer did not get a multisampled target here");

  const auto single = render_edge(api, "rr-fixk-edge-samples1.fs");
  INFO("error=" << single.err);
  REQUIRE(single.err.empty());

  INFO("renderer samples " << msaa.rendererSamples << ", undeclared: partial="
                           << msaa.partial << " lit=" << msaa.lit
                           << ", SAMPLES 1: partial=" << single.partial
                           << " lit=" << single.lit);
  CHECK(msaa.lit > 500);
  CHECK(single.lit > 500);
  CHECK(msaa.partial > 20);
  CHECK(single.partial == 0);
}

namespace
{
// position at `positionBinding`, colour at `colourBinding`, over a geometry
// publishing two streams.
ossia::geometry fixkGeometry(int positionBinding, int colourBinding)
{
  ossia::geometry geom;
  geom.bindings.push_back({12, ossia::geometry::binding::per_vertex, 0});
  geom.bindings.push_back({16, ossia::geometry::binding::per_vertex, 0});

  ossia::geometry::attribute pos;
  pos.binding = positionBinding;
  pos.location = 0;
  pos.format = ossia::geometry::attribute::float3;
  pos.semantic = ossia::attribute_semantic::position;
  geom.attributes.push_back(pos);

  ossia::geometry::attribute col;
  col.binding = colourBinding;
  col.location = 1;
  col.format = ossia::geometry::attribute::float4;
  col.semantic = ossia::attribute_semantic::color0;
  geom.attributes.push_back(col);
  return geom;
}
}

TEST_CASE(
    "the strict vertex-input resolver refuses an out-of-range geometry binding",
    "[gfx][vertex-input][fixK]")
{
  using namespace score::gfx;

  bool ran = false;
  bool inRange = false, oneOut = true, allOut = true;
  std::string error;
  const auto api = platform_backends().front();
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    auto state = createRenderState(api, QSize(16, 16), nullptr);
    if(!state || !state->rhi)
    {
      error = "RHI unavailable";
      if(state)
        state->destroy();
      return;
    }
    auto& rhi = *state->rhi;
    {
      const auto shaders = makeShaders(*state, QStringLiteral(R"_(#version 450
layout(location=0) in vec3 position;
layout(location=1) in vec4 color0;
layout(location=0) out vec4 v_col;
void main() { gl_Position = vec4(position, 1.0); v_col = color0; }
)_"),
          QStringLiteral(R"_(#version 450
layout(location=0) in vec4 v_col;
layout(location=0) out vec4 frag;
void main() { frag = v_col; }
)_"));

      std::unique_ptr<QRhiShaderResourceBindings> srb(rhi.newShaderResourceBindings());
      srb->setBindings({});
      srb->create();

      QRhiVertexInputLayout seed;
      seed.setBindings(
          {{12, QRhiVertexInputBinding::PerVertex},
           {16, QRhiVertexInputBinding::PerVertex}});
      seed.setAttributes({{0, 0, QRhiVertexInputAttribute::Float3, 0}});

      const auto run = [&](const ossia::geometry& geom) {
        std::unique_ptr<QRhiGraphicsPipeline> p(rhi.newGraphicsPipeline());
        p->setShaderResourceBindings(srb.get());
        p->setVertexInputLayout(seed);
        FallbackBindingPlan plan;
        return remapPipelineVertexInputs(*p, shaders.first, geom, &plan);
      };

      inRange = run(fixkGeometry(0, 1));
      // Colour past the two prepared bindings, position still in range: it
      // used to be renumbered onto position's stream.
      oneOut = run(fixkGeometry(0, 5));
      // Nothing in range: a layout with no binding at all, which
      // VUID-VkPipelineVertexInputStateCreateInfo-binding-00615 forbids.
      allOut = run(fixkGeometry(4, 5));
      ran = true;
    }
    state->destroy();
  });

  INFO(error);
  REQUIRE(ran);
  CHECK(inRange);
  CHECK(!oneOut);
  CHECK(!allOut);
}
