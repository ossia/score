// A PER_CUBE_FACE raw raster that declares no depth OUTPUT still depth-tests
// every face against the scratch depth each face pass attaches, with the
// shader's DEPTH_COMPARE.
//
// rr-cube-depth-implicit draws a green quad at window depth 0.8, then a red one
// at 0.2, into every face: the default reverse-Z compare (greater) keeps green.
// rr-cube-depth-implicit-less draws red at 0.2, then green at 0.8: DEPTH_COMPARE
// "less" keeps red. In both the quad that must win is drawn first, so a pass
// without depth testing shows the other one. Before the fix the pipeline was
// built against the node's layered target, which has no depth attachment, so
// depth testing was off and every face showed the last quad drawn.
// The six faces are read through syn-cube-six-probe.fs (3x2 grid of axis
// probes).
#include <score_test/Gfx.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <string>

using namespace score::test::gfx;

namespace
{
QString corpus(const QString& file)
{
  return QString{GFX_TEST_CORPUS_DIR "/"} + file;
}

struct Result
{
  bool skipped{};
  std::string error;
  ReadbackImage view;
};

Result run(score::gfx::GraphicsApi be, const QString& name)
{
  Result r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int prod = p.addRaster(corpus(name + ".vs"), corpus(name + ".fs"));
    const int view = p.addIsf(corpus("syn-cube-six-probe.fs"));
    if(prod < 0 || view < 0)
    {
      r.error = p.error().empty() ? "node build failed" : p.error();
      return;
    }
    const int sink = p.addSink({96, 64});
    p.wire(p.imageOut(prod, 0), p.imageIn(view, 0));
    p.wire(p.imageOut(view, 0), p.sinkInput(sink));
    if(!p.create(be))
    {
      r.skipped = p.skipped();
      r.error = r.skipped ? std::string{} : p.error();
      return;
    }
    p.render(3);
    r.view = p.readback(sink);
    if(r.error.empty())
      r.error = p.error();
  });
  return r;
}

void checkFaces(const ReadbackImage& img, bool green)
{
  for(int cy = 0; cy < 2; cy++)
    for(int cx = 0; cx < 3; cx++)
    {
      const auto px = img.at(cx * img.width / 3 + img.width / 6, cy * img.height / 2 + img.height / 4);
      INFO("cell " << cx << "," << cy << " = " << int(px[0]) << "," << int(px[1]) << ","
                   << int(px[2]));
      if(green)
      {
        CHECK(px[1] > 200);
        CHECK(px[0] < 50);
      }
      else
      {
        CHECK(px[0] > 200);
        CHECK(px[1] < 50);
      }
    }
}
}

TEST_CASE(
    "a PER_CUBE_FACE pass without a depth output depth-tests every face",
    "[gfx][cubemap][depth]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(be));
  const auto r = run(be, QStringLiteral("rr-cube-depth-implicit"));
  if(r.skipped)
    SKIP("backend unavailable");
  INFO(r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.view.valid());
  checkFaces(r.view, true);
}

TEST_CASE(
    "a PER_CUBE_FACE pass without a depth output honours DEPTH_COMPARE",
    "[gfx][cubemap][depth]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(be));
  const auto r = run(be, QStringLiteral("rr-cube-depth-implicit-less"));
  if(r.skipped)
    SKIP("backend unavailable");
  INFO(r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.view.valid());
  checkFaces(r.view, false);
}
