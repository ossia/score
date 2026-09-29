// =============================================================================
// L3 ISF feature surface -- PERSISTENT / FEEDBACK.
//
// A PERSISTENT pass keeps its target across frames, reading the decayed previous
// frame and stamping a dot. Rendering N frames must make the accumulation advance
// with frame count, which is the signal that the ping-pong carries state forward,
// and the advance must agree across backends.
//
// The fixture feeds `date` in flicks, so ISF TIME is about 0 and the dot is
// stationary at uv (0.8, 0.5), pixel ~(51,32). The assertion is therefore the
// per-frame accumulation at that fixed dot -- the delta between a short and a
// long run -- not an absolute early-frame value, so a legitimate warmup
// difference between backends cannot false-fail it.
//
// Covers isf-persistent-feedback.fs, a single-pass PERSISTENT+FLOAT trail buffer.
// The camera-UBO variant needs an upstream Buffer producer the fixture cannot
// feed, and feedback-texture.fs needs a Delayed edge from a node's output back to
// its own input, a topology the linear-chain fixture cannot build; both SKIP, see
// IsfUnsupported.cpp.
// =============================================================================
#include "IsfTestCommon.hpp"

#include <algorithm>

using namespace score::test::gfx;
using namespace score::test::gfx::isf;

namespace
{
// Dot pixel for the stationary (TIME≈0) trail: uv≈(0.8,0.5).
constexpr int kDotX = 51, kDotY = 32;

std::array<int, 4> ints(std::array<uint8_t, 4> p)
{
  return {p[0], p[1], p[2], p[3]};
}
}

// -----------------------------------------------------------------------------
// isf-persistent-feedback: the trail buffer accumulates the (stationary) dot
// each frame. With persistence working, the dot's red channel RISES from a
// short run to a long run (1 frame -> ~181, 8 frames -> saturated 255: the
// output pass sees the trail its persistent pass rendered in the same frame, so
// a 2-frame run would already saturate). If the persistent ping-pong were
// broken (each frame a fresh single dot), the two runs would be identical. So
// dot(long) > dot(short) is the persistence proof.
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
    auto r1 = score::test::gfx::render_isf_chain(
        backend, {corpus("isf-persistent-feedback.fs")}, {64, 64}, 1);
    auto r8 = score::test::gfx::render_isf_chain(
        backend, {corpus("isf-persistent-feedback.fs")}, {64, 64}, 8);
    out.skipped = r1.skipped;
    out.skip_reason = r1.skip_reason;
    out.backend = r1.backend;
    out.error = r1.error.empty() ? r8.error : r1.error;
    if(!r1.outputs.empty())
      out.shortRun = r1.outputs[0];
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
  INFO("dot RED short(1 frame)=" << rShort << " long(8 frames)=" << rLong);

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

// -----------------------------------------------------------------------------
// A PERSISTENT *last* pass is copied to the output after it ran in the same
// frame, so the output shows that frame. The shader adds 16/255 to its previous
// red each frame: after k frames the output is k*16. Showing the texture the
// pass read instead of the one it wrote gives (k-1)*16 (and a blank frame 1).
// -----------------------------------------------------------------------------
TEST_CASE(
    "a persistent last pass shows the frame it just rendered",
    "[gfx][l3][isf][persistent]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));
  const int frames = GENERATE(1, 2, 3, 4);
  CAPTURE(frames);

  const auto r
      = render(backend, {corpus("isf-persistent-last-pass-accumulate.fs")}, {64, 64}, frames);
  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);
  INFO(r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() == 1);
  REQUIRE(r.outputs[0].valid());
  CAPTURE(ints(r.outputs[0].center()));
  CHECK(near(r.outputs[0].center(), {uint8_t(16 * frames), 0, 0, 255}, 3));
}

// -----------------------------------------------------------------------------
// Same property with a value that does not depend on persistence at all: a
// persistent last pass writing FRAMEINDEX reads back exactly like the same
// shader without PASSES, which draws straight into the output.
// -----------------------------------------------------------------------------
TEST_CASE(
    "a persistent last pass writing FRAMEINDEX matches the non-persistent shader",
    "[gfx][l3][isf][persistent]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));
  const int frames = GENERATE(1, 2, 3);
  CAPTURE(frames);

  const auto persistent = render(
      backend, {corpus("isf-persistent-last-pass-frameindex.fs")}, {64, 64}, frames);
  const auto plain = render(backend, {corpus("isf-frameindex.fs")}, {64, 64}, frames);
  if(persistent.skipped)
    SKIP(persistent.backend + ": " + persistent.skip_reason);
  INFO(persistent.error << plain.error);
  REQUIRE(persistent.error.empty());
  REQUIRE(plain.error.empty());
  REQUIRE(persistent.outputs.size() == 1);
  REQUIRE(plain.outputs.size() == 1);
  REQUIRE(persistent.outputs[0].valid());
  REQUIRE(plain.outputs[0].valid());
  CAPTURE(ints(persistent.outputs[0].center()), ints(plain.outputs[0].center()));
  // FRAMEINDEX / 255 is exact in an 8-bit target: a one-frame lag is a
  // difference of exactly 1, so no tolerance.
  CHECK(near(persistent.outputs[0].center(), plain.outputs[0].center(), 0));
}

// -----------------------------------------------------------------------------
// ISF v1: persistence declared by the top-level PERSISTENT_BUFFERS list, the
// pass only naming the buffer as its TARGET. The parser turns it into a v2
// PERSISTENT pass, so the v1 shader accumulates exactly like its v2 twin. Left
// non-persistent, it would draw 16 every frame from a never-written sampler.
// -----------------------------------------------------------------------------
TEST_CASE(
    "an ISF v1 PERSISTENT_BUFFERS shader accumulates like its v2 twin",
    "[gfx][l3][isf][persistent]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));
  const int frames = GENERATE(2, 4);
  CAPTURE(frames);

  const auto r = render(
      backend, {corpus("isf-v1-persistent-buffers-accumulate.fs")}, {64, 64}, frames);
  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);
  INFO(r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() == 1);
  REQUIRE(r.outputs[0].valid());
  CAPTURE(ints(r.outputs[0].center()));
  CHECK(near(r.outputs[0].center(), {uint8_t(16 * frames), 0, 0, 255}, 3));
}

// -----------------------------------------------------------------------------
// Readers of a persistent pass in a multi-pass shader, as in VVISF: a pass
// drawn AFTER the persistent pass sees what it rendered in the same frame; the
// persistent pass reading itself, and a pass drawn BEFORE it, see the previous
// frame. The persistent pass adds 16/255 to its own previous red, so after k
// frames it holds k*16, and a reader shows either k*16 (same frame) or
// (k-1)*16 (previous frame). The frames go through both halves of the texture
// ping-pong twice, so a reader that alternates between two latencies fails.
// -----------------------------------------------------------------------------
namespace
{
IsfResult renderPersistent(
    score::gfx::GraphicsApi backend, const char* shader, int frames)
{
  return render(backend, {corpus(shader)}, {64, 64}, frames);
}

uint8_t level(int steps)
{
  return uint8_t(16 * std::max(steps, 0));
}
}

TEST_CASE(
    "passes after a persistent pass see the frame it just rendered",
    "[gfx][l3][isf][persistent]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));
  const int k = GENERATE(1, 2, 3, 4);
  CAPTURE(k);

  // [acc (persistent, reads itself), copy <- acc, output <- (acc, copy)].
  const auto r = renderPersistent(backend, "isf-persistent-read-after-writer.fs", k);
  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);
  INFO(r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() == 1);
  REQUIRE(r.outputs[0].valid());
  CAPTURE(ints(r.outputs[0].center()));
  CHECK(near(r.outputs[0].center(), {level(k), level(k), 0, 255}, 3));
}

TEST_CASE(
    "passes before a persistent pass see its previous frame",
    "[gfx][l3][isf][persistent]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));
  const int k = GENERATE(1, 2, 3, 4);
  CAPTURE(k);

  // [early <- acc, spacer, acc (persistent, reads itself),
  //  output <- (acc, early, spacer)].
  const auto r = renderPersistent(backend, "isf-persistent-read-before-writer.fs", k);
  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);
  INFO(r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() == 1);
  REQUIRE(r.outputs[0].valid());
  CAPTURE(ints(r.outputs[0].center()));
  CHECK(near(r.outputs[0].center(), {level(k), level(k - 1), 128, 255}, 3));
}

TEST_CASE(
    "a MultiFrame-like chain of persistent passes shows the frames in order",
    "[gfx][l3][isf][persistent]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));
  const int k = GENERATE(1, 2, 3, 4, 5);
  CAPTURE(k);

  // [b3 <- b2, b2 <- b1, b1 (reads itself), output <- (b1, b2, b3)], all b*
  // persistent: each copy is drawn before its source, so it lags one frame.
  const auto r = renderPersistent(backend, "isf-persistent-delay-line.fs", k);
  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);
  INFO(r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() == 1);
  REQUIRE(r.outputs[0].valid());
  CAPTURE(ints(r.outputs[0].center()));
  CHECK(near(r.outputs[0].center(), {level(k), level(k - 1), level(k - 2), 255}, 3));
}

TEST_CASE(
    "persistent passes copying the one drawn before them agree in the same frame",
    "[gfx][l3][isf][persistent]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));
  const int k = GENERATE(1, 2, 3, 4);
  CAPTURE(k);

  // [b1 (reads itself), b2 <- b1, b3 <- b2, shown <- (b1, b2, b3)], every
  // pass persistent, so the output is also the blit of a persistent last pass.
  const auto r = renderPersistent(backend, "isf-persistent-forward-chain.fs", k);
  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);
  INFO(r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() == 1);
  REQUIRE(r.outputs[0].valid());
  CAPTURE(ints(r.outputs[0].center()));
  CHECK(near(r.outputs[0].center(), {level(k), level(k), level(k), 255}, 3));
}
