// =============================================================================
// L3 ISF feature surface — PERSISTENT / FEEDBACK.
//
// A PERSISTENT pass keeps its target across frames; each frame reads the decayed
// previous frame and stamps a dot. We render N frames and assert the persistent
// accumulation ADVANCES with frame count (steady-state grows), which is the
// signal that the ping-pong actually carries state forward — and that the
// advance AGREES across backends.
//
// Determinism note (measured): the fixture feeds `date` in flicks, so ISF TIME
// is ~0 and the dot is stationary at uv≈(0.8,0.5) (pixel ~(51,32)); we therefore
// assert the per-frame ACCUMULATION at that fixed dot (delta between a short and
// a long run), NOT any absolute early-frame value — a legitimate warmup-phase
// difference between backends won't false-fail this.
//
// Corpus covered here:
//   * isf-persistent-feedback.fs — single-pass PERSISTENT+FLOAT trail buffer.
//
// The camera-UBO persistent variant (isf-persistent-uniform-input.fs) needs an
// upstream Buffer/camera producer the fixture cannot feed → SKIP (see
// IsfUnsupported.cpp). The cross-node self-feedback shader (feedback-texture.fs)
// needs a Delayed edge from the node's output back to its own input, a topology
// the linear-chain fixture cannot build → SKIP (see IsfUnsupported.cpp).
//
// Run: DISPLAY=:0 ctest -R gfx --output-on-failure
// =============================================================================
#include "IsfTestCommon.hpp"

using namespace score::test::gfx;
using namespace score::test::gfx::isf;

namespace
{
// Dot pixel for the stationary (TIME≈0) trail: uv≈(0.8,0.5).
constexpr int kDotX = 51, kDotY = 32;
}

// -----------------------------------------------------------------------------
// isf-persistent-feedback: the trail buffer accumulates the (stationary) dot
// each frame. With persistence working, the dot's red channel RISES from a
// short run to a long run (2 frames -> ~181, 8 frames -> saturated 255). If the
// persistent ping-pong were broken (each frame a fresh single dot), the two runs
// would be identical. So dot(long) > dot(short) is the persistence proof.
// -----------------------------------------------------------------------------
TEST_CASE(
    "isf-persistent-feedback accumulates across frames", "[gfx][l3][isf][persistent]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  struct Run
  {
    bool skipped = false;
    std::string skip_reason, backend, error;
    ReadbackImage shortRun, longRun;
  } out;

  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    auto r2 = score::test::gfx::render_isf_chain(
        backend, {corpus("isf-persistent-feedback.fs")}, {64, 64}, 2);
    auto r8 = score::test::gfx::render_isf_chain(
        backend, {corpus("isf-persistent-feedback.fs")}, {64, 64}, 8);
    out.skipped = r2.skipped;
    out.skip_reason = r2.skip_reason;
    out.backend = r2.backend;
    out.error = r2.error.empty() ? r8.error : r2.error;
    if(!r2.outputs.empty())
      out.shortRun = r2.outputs[0];
    if(!r8.outputs.empty())
      out.longRun = r8.outputs[0];
  });

  if(out.skipped)
    SKIP(out.backend + ": " + out.skip_reason);
  INFO("backend=" << out.backend);
  REQUIRE(out.error.empty());
  REQUIRE(out.shortRun.valid());
  REQUIRE(out.longRun.valid());

  const int rShort = out.shortRun.at(kDotX, kDotY)[0];
  const int rLong = out.longRun.at(kDotX, kDotY)[0];
  INFO("dot RED short(2 frames)=" << rShort << " long(8 frames)=" << rLong);

  // Dot is present in both runs (feedback pass rendered the stamp).
  CHECK(rShort > 80);
  // Persistent accumulation advances the stored value across frames.
  CHECK(rLong > rShort + 20);

  // Background (far from the dot) stays dark — the trail is localized.
  const auto bg = out.longRun.at(4, 4);
  INFO("background = (" << (int)bg[0] << "," << (int)bg[1] << "," << (int)bg[2] << ")");
  CHECK(bg[0] < 40);
  CHECK(bg[1] < 40);
}

// -----------------------------------------------------------------------------
// The steady-state readback (long run) must AGREE across backends: the dot's
// accumulated value should not depend on GL vs Vulkan.
// -----------------------------------------------------------------------------
TEST_CASE(
    "isf-persistent-feedback steady state agrees across backends",
    "[gfx][l3][isf][persistent]")
{
  std::vector<ReadbackImage> got;
  for(auto api : platform_backends())
  {
    IsfResult r;
    score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
      r = score::test::gfx::render_isf_chain(
          api, {corpus("isf-persistent-feedback.fs")}, {64, 64}, 8);
    });
    if(!r.skipped && r.error.empty() && !r.outputs.empty() && r.outputs[0].valid())
      got.push_back(r.outputs[0]);
  }
  if(got.size() < 2)
    SKIP("need >=2 live backends to compare");
  for(std::size_t i = 1; i < got.size(); ++i)
  {
    const int d = max_channel_diff(got[0], got[i]);
    INFO("steady-state max channel diff vs backend0 = " << d);
    CHECK(d <= 6); // small warmup/rounding tolerance
  }
}
