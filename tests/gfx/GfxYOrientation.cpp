// =============================================================================
// Y-ORIENTATION PROBE — where does NDC +Y land in the readback, per backend?
//
// Registration: score_add_gfx_test(y_orientation GfxYOrientation.cpp)
//
// WHY THIS EXISTS. crousti_cpu_nodes' per_draws case and the indirect-draw
// benchmark both read back VERTICALLY FLIPPED on Metal, while three other
// Metal tests (orientation_findings, csf_orient_macros, cubemap_six_faces) are
// correctly oriented. Changing InvertYRenderer's per-backend branch fixes the
// first pair and breaks the second three, so the inconsistency is NOT a global
// per-backend rule — some path flips and some does not.
//
// Reasoning about it failed twice: a model built on NDC direction plus
// framebuffer origin predicts all three backends net out identically, which
// contradicts the measurement. Measured conventions, for the record:
//
//     backend | isYUpInFramebuffer | isYUpInNDC | clipSpaceCorrMatrix Y
//     OpenGL  |         1          |     1      |  +1  (identity)
//     Vulkan  |         0          |     0      |  -1  (negates Y)
//     Metal   |         0          |     1      |  +1  (depth only)
//
// So this file measures instead of deriving. It draws the SIMPLEST possible
// thing — a procedural triangle covering exactly the +Y half of NDC, no
// geometry input, no scene chain, no CSF — and reports which rows are lit.
// That isolates the base raster->readback path from the geometry path.
//
// READ THE RESULT LIKE THIS:
//   * all backends light the same rows  -> the base path is consistent, and the
//     Metal flip is introduced by the SCENE/CSF-GEOMETRY chain, not here.
//   * Metal lights the opposite rows    -> the flip is in the base path, and
//     the three currently-green orientation tests are green for some other
//     reason that must then be explained before anything is changed.
//
// The test does not assert a backend-specific expectation: it asserts only
// that SOMETHING was drawn, then REPORTS the lit band via INFO so the number
// is visible on every backend. Pinning an expectation before the mechanism is
// understood is how the last fix attempt broke three tests.
// =============================================================================
#include "IsfTestCommon.hpp"

namespace
{
struct Band
{
  int firstLit = -1;
  int lastLit = -1;
  int litRows = 0;
  int width = 0;
  int height = 0;
};

// A row counts as lit if its centre pixel is bright.
Band bandOf(const score::test::gfx::ReadbackImage& img)
{
  Band b;
  b.width = img.width;
  b.height = img.height;
  for(int y = 0; y < img.height; ++y)
  {
    if(img.at(img.width / 2, y)[0] > 128)
    {
      if(b.firstLit < 0)
        b.firstLit = y;
      b.lastLit = y;
      ++b.litRows;
    }
  }
  return b;
}
}

TEST_CASE("NDC +Y lands on a measurable row band", "[gfx][orientation][yprobe]")
{
  const auto api = GENERATE(from_range(score::test::gfx::platform_backends()));

  Band band;
  std::string err;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    score::test::gfx::GfxPipeline p;
    const int raster
        = p.addRaster(score::test::gfx::isf::corpus("syn-yprobe-ndc.vs"),
          score::test::gfx::isf::corpus("syn-yprobe-ndc.fs"));
    if(raster < 0)
    {
      err = "raster node did not build";
      return;
    }
    const int sink = p.addSink({64, 64});
    p.wire(p.imageOut(raster, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      err = p.error();
      return;
    }
    p.render(4);
    band = bandOf(p.readback(sink));
  });

  if(!err.empty())
    SKIP("y-probe chain unavailable: " << err);

  INFO(
      "backend=" << score::test::gfx::backend_name(api) << " size=" << band.width
                 << "x" << band.height << " litRows=" << band.litRows
                 << " first=" << band.firstLit << " last=" << band.lastLit);

  // Negative control: an orientation oracle is vacuous if nothing drew.
  REQUIRE(band.litRows > 0);
  // The triangle covers half the frame, so roughly half the rows must be lit.
  // This is backend-independent and catches a degenerate draw.
  CHECK(band.litRows >= band.height / 4);
  CHECK(band.litRows <= (3 * band.height) / 4);
}
