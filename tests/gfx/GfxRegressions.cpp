// =============================================================================
// L3 GPU regression tests. Each renders a real offscreen pipeline on every RHI
// backend available on this machine (GENERATE over platform_backends()) and
// asserts on the read-back pixels; a backend that cannot start here is a
// per-backend SKIP, never a silent drop.
//
//     DISPLAY=:0 ctest -R gfx --output-on-failure
//     SCORE_TEST_API=opengl|vulkan|...   restricts to one backend
// =============================================================================

#include <score_test/Gfx.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

namespace
{
QString corpus(const char* file)
{
  return QString{GFX_TEST_CORPUS_DIR "/"} + file;
}

using score::test::gfx::near;
using score::test::gfx::ReadbackImage;
}

// -----------------------------------------------------------------------------
// ISF persistent-SSBO swap, once per frame rather than once per outgoing edge.
//
// A persistent read_write storage buffer that advances by one each frame is
// wired into a DIAMOND: prod.out0 -> mix.a and prod.out0 -> mix.b, mix -> sink.
// That gives the single prod renderer two outgoing edges inside one RenderList,
// the condition the frame-counter guard covers.
//
// isf-persistent-counter.fs encodes (count mod 16) * 16 as a grey level, so a
// correct swap steps +16/255 per frame. Render, sample, render one more frame,
// sample again, and require the per-frame delta to be one step. Measuring the
// delta rather than an absolute value keeps the assertion independent of the
// buffer's initial contents and of the swap phase.
// -----------------------------------------------------------------------------
TEST_CASE(
    "R3-N1: persistent ISF feeding two edges advances once per frame",
    "[gfx][l3][regression][persistent]")
{
  const auto backend
      = GENERATE(from_range(score::test::gfx::platform_backends()));
  CAPTURE(score::test::gfx::backend_name(backend));

  struct Out
  {
    bool skipped = false;
    std::string skip_reason;
    std::string error;
    std::string backend;
    int grey0 = -1; // grey after N frames
    int grey1 = -1; // grey after N+1 frames
  } out;

  score::test::run_in_gui_app(
      [&](const score::GUIApplicationContext&) {
        score::test::gfx::GfxPipeline p;
        const int prod = p.addIsf(corpus("isf-persistent-counter.fs"));
        const int mix = p.addIsf(corpus("isf-mix-two.fs"));
        const int sink = p.addSink({64, 64});
        if(prod < 0 || mix < 0)
        {
          out.error = p.error();
          return;
        }

        // Diamond: the one persistent output feeds BOTH image inputs of mix.
        p.wire(p.imageOut(prod, 0), p.imageIn(mix, 0)); // prod.out0 -> mix.a
        p.wire(p.imageOut(prod, 0), p.imageIn(mix, 1)); // prod.out0 -> mix.b
        p.wire(p.imageOut(mix, 0), p.sinkInput(sink));  // mix.out0 -> sink

        if(!p.create(backend))
        {
          out.skipped = p.skipped();
          out.skip_reason = p.skipReason();
          out.error = p.error();
          out.backend = p.backend();
          return;
        }
        out.backend = p.backend();

        // Warm up + a few advances, sample, advance one more, sample.
        p.render(4);
        out.grey0 = p.readback(sink).center()[0];
        p.render(1);
        out.grey1 = p.readback(sink).center()[0];
      });

  if(out.skipped)
    SKIP(out.backend + ": " + out.skip_reason);

  INFO("backend=" << out.backend);
  REQUIRE(out.error.empty());
  REQUIRE(out.grey0 >= 0);
  REQUIRE(out.grey1 >= 0);

  // Per-frame advance, handling the mod-16 wrap (…, 240, 0, 16, …).
  int delta = out.grey1 - out.grey0;
  if(delta < 0)
    delta += 256;
  INFO("grey after N frames=" << out.grey0 << " after N+1=" << out.grey1
                              << " delta=" << delta);

  // Once-per-frame advance == one 16-unit step (± 8-bit rounding). The bug
  // would double this to ~32. Require a single step and reject the double.
  CHECK(delta >= 10);
  CHECK(delta <= 22);
}

// -----------------------------------------------------------------------------
// Sink sampler must survive a sibling's last-edge removal.
//
// One producer feeds two BackgroundNode sinks: A.out0 -> B and A.out0 -> C.
// Render, remove the A->B edge mid-run through the same incremental path the app
// uses (Graph::onEdgeRemoved + removeEdge + reconcile), then render more.
// Dropping the last edge into B releases a render target that C's
// shader-resource binding may still sample, so C must keep rendering the
// producer's solid magenta.
// -----------------------------------------------------------------------------
TEST_CASE(
    "P0: removing one fan-out edge mid-run leaves the survivor intact",
    "[gfx][l3][regression][mutation]")
{
  const auto backend
      = GENERATE(from_range(score::test::gfx::platform_backends()));
  CAPTURE(score::test::gfx::backend_name(backend));

  struct Out
  {
    bool skipped = false;
    std::string skip_reason;
    std::string error;
    std::string backend;
    bool survivor_valid = false;
    std::array<uint8_t, 4> survivor_center{};
    std::array<uint8_t, 4> survivor_corner{};
  } out;

  score::test::run_in_gui_app(
      [&](const score::GUIApplicationContext&) {
        score::test::gfx::GfxPipeline p;
        const int a = p.addIsf(corpus("isf-solid-color.fs")); // magenta producer
        if(a < 0)
        {
          out.error = p.error();
          return;
        }
        const int b = p.addSink({64, 64});
        const int c = p.addSink({64, 64});

        auto* aout = p.imageOut(a, 0);
        p.wire(aout, p.sinkInput(b)); // A -> B
        p.wire(aout, p.sinkInput(c)); // A -> C

        if(!p.create(backend))
        {
          out.skipped = p.skipped();
          out.skip_reason = p.skipReason();
          out.error = p.error();
          out.backend = p.backend();
          return;
        }
        out.backend = p.backend();

        // Render with both edges live.
        p.render(3);

        // Remove the last (only) edge into B — B becomes unreachable, but its
        // input render target was shared bookkeeping with C's sampler. This is
        // the operation that used to strand C's sampler on a freed RT.
        p.removeEdgeIncremental(aout, p.sinkInput(b));

        // Keep rendering: C must still produce correct pixels, no crash.
        p.render(3);

        const ReadbackImage cImg = p.readback(c);
        out.survivor_valid = cImg.valid();
        if(cImg.valid())
        {
          out.survivor_center = cImg.center();
          out.survivor_corner = cImg.at(0, 0);
        }
      });

  if(out.skipped)
    SKIP(out.backend + ": " + out.skip_reason);

  INFO("backend=" << out.backend);
  REQUIRE(out.error.empty());
  REQUIRE(out.survivor_valid);

  const std::array<uint8_t, 4> magenta{255, 0, 255, 255};
  INFO(
      "survivor centre = (" << (int)out.survivor_center[0] << ","
                            << (int)out.survivor_center[1] << ","
                            << (int)out.survivor_center[2] << ","
                            << (int)out.survivor_center[3] << ")");
  CHECK(near(out.survivor_center, magenta, 2));
  CHECK(near(out.survivor_corner, magenta, 2));
}

// -----------------------------------------------------------------------------
// Supporting check (not a standalone regression): with two INDEPENDENT
// BackgroundNode sinks fanned off one persistent producer, each sink is its own
// RenderList with its own storage, so both must read the SAME once-per-frame
// value. This documents WHY the R3-N1 reproduction above needs the diamond
// rather than two sinks, and guards the fan-out fixture path.
// -----------------------------------------------------------------------------
TEST_CASE(
    "fan-out: two independent sinks off a persistent producer agree",
    "[gfx][l3][regression][persistent]")
{
  const auto backend
      = GENERATE(from_range(score::test::gfx::platform_backends()));
  CAPTURE(score::test::gfx::backend_name(backend));

  const auto r = [&] {
    score::test::gfx::IsfResult res;
    score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
      res = score::test::gfx::render_isf_fanout(
          backend, {corpus("isf-persistent-counter.fs")}, /*nSinks=*/2);
    });
    return res;
  }();

  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);

  INFO("backend=" << r.backend);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() == 2);
  REQUIRE(r.outputs[0].valid());
  REQUIRE(r.outputs[1].valid());

  const auto c0 = r.outputs[0].center();
  const auto c1 = r.outputs[1].center();
  INFO(
      "sink0 grey=" << (int)c0[0] << " sink1 grey=" << (int)c1[0]);
  // Both independent render lists advanced the same number of frames, so they
  // must show the same grey level.
  CHECK(near(c0, c1, 2));
}
