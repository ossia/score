// =============================================================================
// L3 CSF FEEDBACK LOOP — state must travel around a delayed edge and advance.
//
// This is the shape csf-reaction-diffusion has and the one that broke in three
// different ways over the course of ledger 9.85–9.103. A generator owns the
// geometry, a second node adopts it and transforms it, and a Delayed* cable
// carries the result back to the generator, whose pass-through IS the frame
// transition: `out := in` only advances the state when the owner binds _in and
// _out to different halves of its ping-pong pair.
//
// What broke, each time silently:
//   * the owner re-adopted the shared upstream handle every frame, clobbering
//     ssbo.buffer and undoing its own swap, so its read half was a buffer
//     nothing ever wrote and the pass-through zeroed the state one frame after
//     seeding (ledger 9.99);
//   * the snapshot a gather reads through was allocated one element wide and
//     never resized, so every index past the first read off the end (9.97);
//   * before that, the whole path was aliased and cross-index reads raced.
//
// Every one of those left the picture *changing* but wrong, or frozen at a
// value that still passed the corpus harness's sd >= 0.005 threshold. So the
// assertion here is not "it renders" and not "it is not blank": it is that the
// state ADVANCES monotonically, which is the only property a broken loop
// cannot fake.
//
// FIXTURE: syn-loop-hub.cs owns the geometry and passes the fed-back value
// through; the buffers start zeroed so the loop accumulates from black with no
// seeding logic to depend on. syn-loop-step.cs adopts the geometry and adds
// 0.02 per channel per frame, own-index only. syn-loop-tap.cs copies the result
// out to the raster. wireFeedback(step -> hub) closes the loop on a
// DelayedGlutton edge, so the picture brightens frame by frame.
//
// LOOP LENGTH MATTERS, and finding that out is what this file bought. Closing
// the loop from `step` puts two nodes in the cycle and reverses the render
// order to step -> hub, so the hub reads, in the same frame, the buffer the
// step has just written. On OpenGL that read returned the pre-dispatch
// contents — QRhi inserts no compute-to-compute barrier across
// beginComputePass boundaries there — and the loop settled into a 2-cycle,
// the halves alternating 0.04 / 0.06 forever, while Vulkan and Metal climbed.
// Closing it from `tap`, three nodes, renders hub first and so never reads a
// same-frame write, which is why csf-reaction-diffusion (hub -> Diffuse ->
// React -> hub) never showed it. Each CSF pass now ends with an explicit
// insertComputeBarrier and all three backends return 15 / 56 / 97 here.
//
// NEGATIVE CONTROL, run, and it did NOT catch the adoption defect. Keying the "feedback
// receivers never adopt" guard on `req.gathers` instead of
// `req.access == "read_write"` -- the exact regression of ledger 9.99 -- leaves
// both cases green here. With the defect present the hub is evidently not even
// classified a feedback receiver in this fixture, so the mechanism under test
// is never reached. Two other fixtures were written for the same defect class
// and failed their controls the same way.
//
// So read this file for what it is: a LIVENESS test. It proves state travels a
// delayed edge and accumulates, which catches a loop that dies outright. It
// does NOT protect the owner's pair. That is checked by an invariant in
// RenderedCSFNode::buildComputeSrbBindings, which was verified to fire
// ("feedback receiver is ping-ponging a borrowed buffer") and to stay silent
// on a correct build.
//
// Intended registration: score_add_gfx_test(csf_feedback_loop
// GfxCsfFeedbackLoop.cpp)
//
//   DISPLAY=:0 SCORE_TEST_API=opengl ctest -R gfx_csf_feedback_loop
//   DISPLAY=:0 SCORE_TEST_API=vulkan ctest -R gfx_csf_feedback_loop
//   SCORE_TEST_API=metal ctest -R gfx_csf_feedback_loop
// =============================================================================
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
    // A read-only tap between the loop and the raster, mirroring the Colorize
    // node in csf-reaction-diffusion. Without it the raster consumes the very
    // port the feedback cable leaves from, and on OpenGL the loop then carries
    // about two steps and stops -- a distinct case, noted in the ledger.
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
    // The cable that closes the loop, and the only reason the owner is a
    // feedback receiver at all.
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
// CASE 1 — the state advances around the loop.
//
// Each frame adds 0.02 to every channel, so the drawn level must climb. A loop
// that does not carry leaves the picture at whatever the hub seeded and late
// equals early; a loop that carries but re-seeds every frame behaves the same
// way. Requiring a monotone climb across three samples rules out both.
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
// CASE 2 — the loop is deterministic.
//
// Two independent pipelines rendered for the same number of frames must agree.
// A loop whose owner reads a half nothing wrote, or whose gather reads past the
// end of a truncated buffer, depends on allocation timing rather than on frame
// count, and the two runs drift apart.
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
// CASE 3 — an EVEN-length loop advances too.
//
// Identical to CASE 1 except the feedback cable leaves from `step` instead of
// `tap`, so the cycle contains two nodes rather than three and the hub reads a
// buffer written earlier in the same frame. That difference alone used to lock
// OpenGL into a 2-cycle while Vulkan and Metal climbed; the missing
// compute-to-compute barrier is described in the header. The simplest feedback
// graph a user can draw — one generator, one effect, a cable back — is the
// even case, so it has to work.
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
