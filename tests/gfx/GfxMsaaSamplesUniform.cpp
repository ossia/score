// MSAA_SAMPLES is the sample count of the pass that reads it. It came from the
// render list's shared output UBO, so a raw raster pass declaring SAMPLES on
// its OUTPUTS read the renderer's count instead, and OpenPBR's
// alpha-to-coverage built its coverage mask for the wrong sample count
// (shader board U24).
#include <Gfx/Settings/Model.hpp>

#include <score_test/Gfx.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <cmath>

using namespace score::test::gfx;

namespace
{
QString corpus(const char* file)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR) + QStringLiteral("/") + file;
}

struct samples_guard
{
  Gfx::Settings::Model& gfx;
  int previous{gfx.getSamples()};
  ~samples_guard() { gfx.setSamples(previous); }
};

struct Shot
{
  bool skipped = false;
  std::string err;
  int rendererSamples = 0;
  int red = -1;
};

Shot render(score::gfx::GraphicsApi api, int rendererSamples, const char* fs)
{
  Shot r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext& ctx) {
    samples_guard guard{ctx.settings<Gfx::Settings::Model>()};
    guard.gfx.setSamples(rendererSamples);

    GfxPipeline p;
    const int raster
        = p.addRaster(corpus("rr-msaa-samples-uniform.vs"), corpus(fs));
    if(raster < 0)
    {
      r.err = p.error();
      return;
    }
    const int view = p.addIsf(corpus("msaa-input-view.fs"));
    if(view < 0)
    {
      r.err = p.error();
      return;
    }
    const int sink = p.addSink({32, 32});
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
    r.red = img.center()[0];
  });
  return r;
}

int expected_red(int samples)
{
  return int(std::lround(samples / 16.0 * 255.0));
}
}

TEST_CASE("MSAA_SAMPLES is the sample count the pass renders at", "[gfx][raster][msaa][U24]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  struct Case
  {
    int renderer;
    const char* fs;
    int pass; // 0: the renderer's count
  };
  const auto c = GENERATE(
      Case{4, "rr-msaa-samples-uniform.fs", 0},
      Case{4, "rr-msaa-samples-uniform-s1.fs", 1},
      Case{1, "rr-msaa-samples-uniform-s4.fs", 4},
      Case{1, "rr-msaa-samples-uniform.fs", 0});
  CAPTURE(c.renderer, c.fs);

  const auto s = render(api, c.renderer, c.fs);
  if(s.skipped)
    SKIP("backend unavailable");
  INFO("error=" << s.err);
  REQUIRE(s.err.empty());
  // Not a SKIP, which would report the whole case skipped and hide the other
  // cases' results.
  if(c.renderer > 1 && s.rendererSamples < 2)
  {
    WARN("the renderer did not get a multisampled target here");
    return;
  }

  const int pass = c.pass ? c.pass : s.rendererSamples;
  INFO("renderer samples " << s.rendererSamples << ", pass samples " << pass
                           << ", red " << s.red);
  CHECK(std::abs(s.red - expected_red(pass)) <= 2);
}
