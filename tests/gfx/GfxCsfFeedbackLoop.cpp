// CSF feedback loop: state travels a delayed geometry edge and accumulates.
//
// A generator owns the geometry, a second node adopts and transforms it, and a
// Delayed* cable carries the result back. The owner's pass-through `out := in`
// is the frame transition, so it only advances when _in and _out are different
// halves of its ping-pong pair.
//
// The assertion is that the drawn level climbs monotonically across three
// samples: a loop that does not carry, and one that re-seeds every frame, both
// leave it flat while still rendering something.
//
// Fixtures: syn-loop-hub.cs owns the geometry and passes the fed-back value
// through, starting from zeroed buffers so no seeding logic is involved;
// syn-loop-step.cs adopts it and adds 0.02 per channel per frame at its own
// index; syn-loop-tap.cs copies the result out to the raster.
//
// Loop length decides the render order. Closing from `step` puts two nodes in
// the cycle and orders them step -> hub, so the hub reads a buffer written
// earlier in the same frame; closing from `tap` renders the hub first. Both
// shapes are covered, because only the first exercises the compute barrier.
//
// The owner is classified a feedback receiver here, so the ping-pong pair and
// the swap are on the path under test. What the assertion cannot see is the
// pair's identity: a loop that advances while reading the wrong half still
// climbs. That is checked by an invariant in
// RenderedCSFNode::buildComputeSrbBindings.
//
//   DISPLAY=:0 SCORE_TEST_API=opengl ctest -R gfx_csf_feedback_loop
//   DISPLAY=:0 SCORE_TEST_API=vulkan ctest -R gfx_csf_feedback_loop
//   SCORE_TEST_API=metal ctest -R gfx_csf_feedback_loop
#include "GfxIncrementalCommon.hpp"

using namespace score::test::gfx;
using namespace score::test::gfx::isf;
using namespace score::test::gfx::incremental;

namespace
{
constexpr int kSinkPx = 64;

struct LoopShot
{
  bool skipped = false;
  std::string skip_reason, backend, error;
  ReadbackImage early, mid, late;
};

/// Mean of the drawn (non-background) pixels' green channel. The fixture only
/// ever writes greys, and the raster leaves the background black, so this
/// tracks the accumulated value without being diluted by the gaps between
/// triangles the way a whole-frame mean would be.
double drawn_level(const ReadbackImage& img)
{
  if(!img.valid())
    return -1.0;
  double sum = 0.0;
  int n = 0;
  for(int y = 0; y < img.height; ++y)
    for(int x = 0; x < img.width; ++x)
    {
      const auto p = img.at(x, y);
      const int v = int(p[1]);
      if(v > 8)
      {
        sum += v;
        ++n;
      }
    }
  return n > 0 ? sum / n : 0.0;
}

int drawn_pixels(const ReadbackImage& img)
{
  if(!img.valid())
    return 0;
  int n = 0;
  for(int y = 0; y < img.height; ++y)
    for(int x = 0; x < img.width; ++x)
      if(int(img.at(x, y)[1]) > 8)
        ++n;
  return n;
}

LoopShot render_loop(score::gfx::GraphicsApi be, bool closeFromStep = false)
{
  LoopShot r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int hub = p.addCsf(corpus("syn-loop-hub.cs"));
    const int step = p.addCsf(corpus("syn-loop-step.cs"));
    // A tap between the loop and the raster, so the raster does not consume
    // the same port the feedback cable leaves from.
    const int tap = p.addCsf(corpus("syn-loop-tap.cs"));
    const int raster
        = p.addRaster(corpus("raw-raster-basic.vs"), corpus("raw-raster-basic.fs"));
    const int sink = p.addSink({kSinkPx, kSinkPx});
    if(hub < 0 || step < 0 || tap < 0 || raster < 0)
    {
      r.error = p.error();
      return;
    }
    p.wire(p.geometryOut(hub, 0), p.geometryIn(step, 0));
    p.wire(p.geometryOut(step, 0), p.geometryIn(tap, 0));
    p.wire(p.geometryOut(tap, 0), p.geometryIn(raster, 0));
    p.wire(p.imageOut(raster, 0), p.sinkInput(sink));
    p.wireFeedback(
        p.geometryOut(closeFromStep ? step : tap, 0), p.geometryIn(hub, 0));

    if(!p.create(be))
    {
      r.skipped = p.skipped();
      r.skip_reason = p.skipReason();
      r.backend = p.backend();
      r.error = p.error();
      return;
    }
    r.backend = p.backend();

    p.render(4);
    r.early = p.readback(sink);
    p.render(8);
    r.mid = p.readback(sink);
    p.render(8);
    r.late = p.readback(sink);
    if(r.error.empty())
      r.error = p.error();
  });
  return r;
}
}

// -----------------------------------------------------------------------------
// CASE 1 - the state advances around the loop.
// -----------------------------------------------------------------------------
TEST_CASE("state travels around a delayed geometry edge and advances", "[gfx][l3][csf][feedback]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  const LoopShot s = render_loop(backend);
  if(s.skipped)
    SKIP(s.backend + ": " + s.skip_reason);
  if(const char* why = compute_shader_skip_reason(backend))
    SKIP(std::string{backend_name(backend)} + ": " + why);
  CAPTURE(s.backend);
  REQUIRE(s.error.empty());
  REQUIRE(s.early.valid());
  REQUIRE(s.mid.valid());
  REQUIRE(s.late.valid());

  // The grid is actually drawn, so a climb cannot be read off an empty frame.
  REQUIRE(drawn_pixels(s.late) > 200);

  const double a = drawn_level(s.early);
  const double b = drawn_level(s.mid);
  const double c = drawn_level(s.late);
  CAPTURE(a, b, c);

  // Monotone, and by a margin well above 8-bit quantisation: 8 frames at 0.02
  // is ~40 levels, so require at least 5 to leave room for saturation at the
  // top end without accepting a stuck picture.
  CHECK(b > a + 5.0);
  CHECK(c > b + 5.0);
}

// -----------------------------------------------------------------------------
// CASE 2 - the loop is a pure function of the frame count: two independent
// pipelines rendered for the same number of frames must agree.
// -----------------------------------------------------------------------------
TEST_CASE("a feedback loop is a pure function of the frame count", "[gfx][l3][csf][feedback]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  const LoopShot x = render_loop(backend);
  if(x.skipped)
    SKIP(x.backend + ": " + x.skip_reason);
  const LoopShot y = render_loop(backend);
  if(y.skipped)
    SKIP(y.backend + ": " + y.skip_reason);
  if(const char* why = compute_shader_skip_reason(backend))
    SKIP(std::string{backend_name(backend)} + ": " + why);

  CAPTURE(x.backend, drawn_level(x.late), drawn_level(y.late));
  REQUIRE(x.error.empty());
  REQUIRE(y.error.empty());
  REQUIRE(x.late.valid());
  REQUIRE(y.late.valid());

  CHECK(std::abs(drawn_level(x.late) - drawn_level(y.late)) < 2.0);
}

// -----------------------------------------------------------------------------
// CASE 3 - an even-length loop advances too. The cable leaves from `step`, so
// the cycle holds two nodes and the hub reads a same-frame write.
// -----------------------------------------------------------------------------
TEST_CASE("an even-length feedback loop advances", "[gfx][l3][csf][feedback]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  const LoopShot s = render_loop(backend, /* closeFromStep */ true);
  if(s.skipped)
    SKIP(s.backend + ": " + s.skip_reason);
  if(const char* why = compute_shader_skip_reason(backend))
    SKIP(std::string{backend_name(backend)} + ": " + why);
  CAPTURE(s.backend);
  REQUIRE(s.error.empty());
  REQUIRE(s.late.valid());
  REQUIRE(drawn_pixels(s.late) > 200);

  const double a = drawn_level(s.early);
  const double b = drawn_level(s.mid);
  const double c = drawn_level(s.late);
  CAPTURE(a, b, c);
  CHECK(b > a + 5.0);
  CHECK(c > b + 5.0);
}
