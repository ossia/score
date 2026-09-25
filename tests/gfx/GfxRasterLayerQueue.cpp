// =============================================================================
// LAYER and QUEUE header keys of raw raster shaders, rendered (the parser and
// bake side is tests/unit/IsfLayerQueueTest):
//
//   * the default resolve composites the first target; the depth test against
//     the consumer's depth hides a quad behind an opaque one although the
//     layer node was created first (transparent sources draw last);
//   * two targets with different blends and clears reach the resolve intact;
//   * a resolve writing gl_FragDepth is seen by a downstream depth reader, and
//     a resolve can read the consumer's depth;
//   * weighted blended OIT written in shader code gives the same colour for
//     either draw order, where the sorted over does not;
//   * QUEUE transparent orders a raster without LAYER; without either, a
//     raster keeps its depth write and its draw order.
//
// Fixture: layer-*.vs draw red alpha 0.5 at window depth 0.3 and green alpha
// 0.6 at 0.6 (red first, or green first for *-gr); layer-opaque-{mid,back}.vs
// an opaque blue triangle at 0.45 or 0.1. layer-view.fs shows the stored rgb on
// the left half and the depth as grey on the right half.
// =============================================================================
#include "IsfTestCommon.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace score::test::gfx;
using namespace score::test::gfx::isf;

namespace
{
struct Shot
{
  bool skipped = false;
  std::string skip_reason, backend, error;
  ReadbackImage image;
};

// layer (and an optional opaque layer created after it) -> view -> sink
Shot render_quads(score::gfx::GraphicsApi be, const char* quadsFs, const char* opaqueVs)
{
  Shot r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const QString quadsVs = QString(quadsFs).replace(".fs", ".vs");
    const int quads = p.addRaster(corpus(quadsVs.toUtf8().constData()), corpus(quadsFs));
    const QString opaqueFs = QString(opaqueVs ? opaqueVs : "").replace(".vs", ".fs");
    const int opaque
        = opaqueVs ? p.addRaster(corpus(opaqueVs), corpus(opaqueFs.toUtf8().constData()))
                   : -2;
    const int view = p.addIsf(corpus("layer-view.fs"));
    const int sink = p.addSink({64, 64});
    if(quads < 0 || opaque == -1 || view < 0)
    {
      r.error = p.error().empty() ? "pipeline build failed" : p.error();
      return;
    }
    p.wire(p.imageOut(quads, 0), p.imageIn(view, 0));
    if(opaque >= 0)
      p.wire(p.imageOut(opaque, 0), p.imageIn(view, 0));
    p.wire(p.imageOut(view, 0), p.sinkInput(sink));
    if(!p.create(be))
    {
      r.skipped = p.skipped();
      r.skip_reason = p.skipReason();
      r.backend = p.backend();
      r.error = p.error();
      return;
    }
    r.backend = p.backend();
    p.render(3);
    r.image = p.readback(sink);
    if(r.error.empty())
      r.error = p.error();
    if(r.error.empty() && !r.image.valid())
      r.error = "empty readback";
  });
  return r;
}

#define LAYER_REQUIRE_LIVE(s)                                \
  if((s).skipped)                                         \
    SKIP((s).backend + ": " + (s).skip_reason);           \
  CAPTURE((s).backend);                                   \
  REQUIRE((s).error.empty());                             \
  REQUIRE((s).image.valid())

std::array<uint8_t, 4> colour(const Shot& s) { return s.image.at(16, 32); }
int depth(const Shot& s) { return s.image.at(48, 32)[0]; }

void check(const Shot& s, std::array<uint8_t, 4> rgb, int d)
{
  const auto c = colour(s);
  INFO("rgb = " << int(c[0]) << "," << int(c[1]) << "," << int(c[2]));
  INFO("depth = " << depth(s));
  CHECK(near(c, rgb, 3));
  CHECK(std::abs(depth(s) - d) <= 2);
}
}

TEST_CASE(
    "the default LAYER resolve composites the first target, tested against the "
    "consumer's depth, after the opaque sources",
    "[gfx][raster][layer]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));
  // green 0.6 over red 0.5: rgb (0.2, 0.6, 0), alpha 0.8; no depth written.
  const Shot alone = render_quads(backend, "layer-quads-layer.fs", nullptr);
  LAYER_REQUIRE_LIVE(alone);
  check(alone, {51, 153, 0, 255}, 0);

  // Opaque blue at 0.45, created after the layer node, hides the red quad at
  // 0.3; green 0.6 over blue: (0, 0.6, 0.4). The consumer's depth stays the
  // opaque one.
  const Shot hidden = render_quads(backend, "layer-quads-layer.fs", "layer-opaque-mid.vs");
  LAYER_REQUIRE_LIVE(hidden);
  check(hidden, {0, 153, 102, 255}, 115);

  // DEPTH_TEST false: the red quad shows too, (0.2, 0.6, 0.2) over blue.
  const Shot notest
      = render_quads(backend, "layer-quads-layer-notest.fs", "layer-opaque-mid.vs");
  LAYER_REQUIRE_LIVE(notest);
  check(notest, {51, 153, 51, 255}, 115);
}

TEST_CASE(
    "two LAYER targets with their own blends and clears reach the resolve",
    "[gfx][raster][layer]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));
  // sum (additive from 0): (0.5, 0.6, 0, 1.1); peak (max from 0.25):
  // (0.5, 0.6, 0.25, 0.6). The resolve writes
  // (sum.r + sum.g - sum.a / 2, peak.g, peak.b) = (0.55, 0.6, 0.25).
  const Shot s = render_quads(backend, "layer-quads-mrt.fs", nullptr);
  LAYER_REQUIRE_LIVE(s);
  check(s, {140, 153, 64, 255}, 0);
}

TEST_CASE(
    "a LAYER resolve writes the consumer's depth, and reads it",
    "[gfx][raster][layer]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));
  // Opaque blue at 0.1 behind both quads: colour (0.2, 0.6, 0.2) every time.
  // The weights of the expected depth: 0.6 (green) and 0.5 * 0.4 = 0.2 (red),
  // accumulated 0.8: (0.6 * 0.6 + 0.2 * 0.3) / 0.8 = 0.525, written where the
  // coverage reaches the threshold input (0.5, not 0.9).
  struct Case
  {
    const char* fs;
    int depth;
  };
  for(auto [fs, d] : {Case{"layer-quads-layer.fs", 26}, Case{"layer-quads-depth.fs", 134},
                      Case{"layer-quads-depth-high.fs", 26}})
  {
    CAPTURE(fs);
    const Shot s = render_quads(backend, fs, "layer-opaque-back.vs");
    LAYER_REQUIRE_LIVE(s);
    check(s, {51, 153, 51, 255}, d);
  }
  // Opaque in front of the red quad: only green contributes, 0.6.
  {
    const Shot s = render_quads(backend, "layer-quads-depth.fs", "layer-opaque-mid.vs");
    LAYER_REQUIRE_LIVE(s);
    check(s, {0, 153, 102, 255}, 153);
  }
  // DEPTH_INPUT: the resolve replaces the consumer with (its depth 0.45, the
  // layer's green 0.6, 0); without an opaque source the depth is the clear.
  {
    const Shot s = render_quads(backend, "layer-quads-depthin.fs", "layer-opaque-mid.vs");
    LAYER_REQUIRE_LIVE(s);
    check(s, {115, 153, 0, 255}, 115);
  }
  {
    const Shot s = render_quads(backend, "layer-quads-depthin.fs", nullptr);
    LAYER_REQUIRE_LIVE(s);
    check(s, {0, 153, 0, 255}, 0);
  }
}

TEST_CASE(
    "weighted blended OIT in shader code is independent of the draw order",
    "[gfx][raster][layer][oit]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));
  // Weights 1 + 9 * nearness: red 3.7, green 6.4. accum = (1.85, 3.84, 0,
  // 5.69), revealage 0.5 * 0.4 = 0.2: (0.3251, 0.6749, 0) * 0.8 over black.
  const Shot rg = render_quads(backend, "layer-oit-rg.fs", nullptr);
  LAYER_REQUIRE_LIVE(rg);
  check(rg, {66, 138, 0, 255}, 0);
  const Shot gr = render_quads(backend, "layer-oit-gr.fs", nullptr);
  LAYER_REQUIRE_LIVE(gr);
  check(gr, {66, 138, 0, 255}, 0);

  // The sorted over depends on it: green over red, red over green.
  const Shot overRg = render_quads(backend, "layer-quads-layer.fs", nullptr);
  LAYER_REQUIRE_LIVE(overRg);
  check(overRg, {51, 153, 0, 255}, 0);
  const Shot overGr = render_quads(backend, "layer-over-gr.fs", nullptr);
  LAYER_REQUIRE_LIVE(overGr);
  check(overGr, {128, 77, 0, 255}, 0);
}

TEST_CASE(
    "QUEUE transparent draws after the opaque sources; without it a raster keeps "
    "its depth write and its draw order",
    "[gfx][raster][layer]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));
  const Shot queued = render_quads(backend, "layer-quads-queue.fs", "layer-opaque-mid.vs");
  LAYER_REQUIRE_LIVE(queued);
  check(queued, {0, 153, 102, 255}, 115);

  const Shot alone = render_quads(backend, "layer-quads-none.fs", nullptr);
  LAYER_REQUIRE_LIVE(alone);
  check(alone, {51, 153, 0, 255}, 153);

  // Created first, so drawn first: its depth 0.6 then rejects the opaque blue
  // at 0.45.
  const Shot first = render_quads(backend, "layer-quads-none.fs", "layer-opaque-mid.vs");
  LAYER_REQUIRE_LIVE(first);
  check(first, {51, 153, 0, 255}, 153);
}
