// =============================================================================
// THE INDIRECT-DRAW FALLBACK LADDER PAINTS THE SAME PICTURE ON EVERY RUNG.
//
// Registration: score_add_gfx_test(indirect_fallback_ladder GfxIndirectFallbackLadder.cpp)
//
// The engine has four ways to consume a GPU-written multi-draw command list,
// from most to least capable (RenderState::Caps declaration, Mesh.cpp /
// CustomMesh.cpp draw ladder):
//
//   R1  drawIndexedIndirectCount / drawIndirectCount -- the draw count is a
//       u32 the GPU wrote into a count buffer (Qt 6.13-era API, detected via
//       RhiIndirectCompat.hpp because the SDK's Qt reports 6.12 while
//       carrying the 6.13 API).
//   R2  one drawIndirect of the full command-slot CAPACITY -- correct only
//       under the producer contract that dead slots stay zeroed.
//   R3  a per-command drawCount=1 indirect loop (no DrawIndirectMulti).
//   R4  CPU loop over commands read back synchronously; the count buffer is
//       read back too and CLAMPS the command list.
//
// And two ways to size an EXECUTION_MODEL INDIRECT compute pass:
//   D1  cb.dispatchIndirect on the args buffer (Qt 6.13-era API),
//   D2  a CPU dispatch of the declared worst-case WORKGROUPS ceiling, the
//       shader self-bounding by reading its own args.
//
// Each rung is forced by an environment kill switch consumed in
// RenderState::Caps::populate (SCORE_GFX_NO_GPU_INDIRECT_COUNT,
// SCORE_GFX_NO_GPU_INDIRECT_MULTI, SCORE_GFX_NO_GPU_INDIRECT,
// SCORE_GFX_NO_GPU_DISPATCH_INDIRECT), and every session must produce the
// exact per-strip picture for its rung -- a rung that silently draws nothing,
// the wrong count, or ignores the GPU-written data goes RED, not skipped.
//
// POSITIVE CONTROLS (a broken kill switch must not hide behind correct
// pixels):
//  * Caps::populate prints one greppable line with the post-switch caps
//    ("score.gfx: RHI indirect caps: ..."); each session captures it through
//    a message handler and REQUIREs the switch actually landed.
//  * The poison device: the compute producer fills dead command slots with
//    draws of sentinel-blue quads (columns 32..63) when 'poison'=1. The
//    count rung (R1) and the clamping CPU rung (R4) must NOT paint them; the
//    capacity rungs (R2, R3) MUST -- which simultaneously proves the count
//    rung was really off in those sessions.
//  * EXECUTION_MODEL INDIRECT passes log one "CSF indirect dispatch:
//    gpu|cpu-fallback" line per node; sessions assert the path they forced.
//
// Qt-tier behaviour (all asserted through the same captured-caps logic):
//   6.4  -- no indirect API at all: every session collapses to R4/D2, the
//           caps line reads all-zero, pictures stay correct.
//   6.12 -- R2/R3 exist, R1/D1 do not (count=0 dispatchIndirect=0 in the
//           caps line without any switch; SCORE_GFX_SIMULATE_QT612 builds
//           compile this tier against a newer Qt).
//   6.13/SDK -- everything present.
//
// The oracle: 64x64 frame, real strip q on pixel columns [4q, 4q+4) reading
// back {255, q, 0, 255}; poison strip j on columns [32+4j, 32+4j+4) reading
// back {0, 0, 255, 255}. The CPU-visible geometry is 96 plain vertices, so a
// draw bypassing the indirect machinery entirely paints all 16 quads and
// fails both families of assertions at once.
// =============================================================================
#include "IsfTestCommon.hpp"

#include <QtCore/qlogging.h>

#include <regex>
#include <string>

namespace
{
using namespace score::test::gfx;
using namespace score::test::gfx::isf;

constexpr int kSize = 64;
constexpr int kStripPx = 4;
constexpr int kTol = 2;
constexpr int kSlots = 8;    // command-slot capacity == maxDrawCount
constexpr int kCountA = 3;
constexpr int kCountB = 6;
constexpr int kCountPort = 0;
constexpr int kPoisonPort = 1;
constexpr int kFrames = 3;

std::array<uint8_t, 4> real_px(int q)
{
  return {255, uint8_t(q), 0, 255};
}
constexpr std::array<uint8_t, 4> kPoisonPx{0, 0, 255, 255};

// Captures qDebug/qWarning text for the duration of one session so the test
// can read the caps line and the CSF dispatch-path line. The previous handler
// is chained and restored (the fixture's own logging keeps working).
struct LogCapture
{
  static inline std::string text;
  static inline QtMessageHandler prev = nullptr;
  static void handler(QtMsgType t, const QMessageLogContext& c, const QString& m)
  {
    text += m.toStdString();
    text += '\n';
    if(prev)
      prev(t, c, m);
  }
  LogCapture()
  {
    text.clear();
    prev = qInstallMessageHandler(&handler);
  }
  ~LogCapture()
  {
    qInstallMessageHandler(prev);
    prev = nullptr;
  }
};

struct LadderResult
{
  bool skipped = false;
  std::string skip_reason;
  std::string error;
  std::string backend;

  // Parsed from the captured "score.gfx: RHI indirect caps:" line -- the
  // POST-kill-switch capabilities the session actually rendered with.
  bool capsParsed = false;
  int capsDrawIndirect = -1, capsMulti = -1, capsCount = -1, capsDispatch = -1;

  // "gpu" / "cpu-fallback" from "score.gfx: CSF indirect dispatch:"; empty
  // when the line never appeared (INDIRECT pass never dispatched -- a bug).
  std::string dispatchLog;

  ReadbackImage at3, at6;
};

const char* const kSwitches[] = {
    "SCORE_GFX_NO_GPU_INDIRECT",
    "SCORE_GFX_NO_GPU_INDIRECT_COUNT",
    "SCORE_GFX_NO_GPU_INDIRECT_MULTI",
    "SCORE_GFX_NO_GPU_DISPATCH_INDIRECT",
};

/// One full engine session under the given kill switches: build the
/// CSF -> raster -> sink chain, render at count=3 and count=6 with the given
/// poison mode, read both frames back, and capture the caps/dispatch logs.
/// Collect only; Catch2 macros run after run_in_gui_app returns.
LadderResult run_session(
    score::gfx::GraphicsApi be, std::initializer_list<const char*> switches,
    int poison)
{
  LadderResult r;
  r.backend = backend_name(be);
  for(const char* s : kSwitches)
    qunsetenv(s);
  for(const char* s : switches)
    qputenv(s, "1");

  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    LogCapture capture;

    GfxPipeline p;
    const int csf = p.addIsf(corpus("syn-indirect-ladder.cs"));
    const int raster
        = p.addRaster(corpus("syn-vertex-color.vs"), corpus("syn-vertex-color.fs"));
    if(csf < 0 || raster < 0)
    {
      r.error = "node build failed: " + p.error();
      return;
    }
    auto* gout = p.geometryOut(csf, 0);
    auto* gin = p.geometryIn(raster, 0);
    if(!gout || !gin)
    {
      r.error = gout ? "raster node has no Geometry input port"
                     : "CSF producer has no Geometry output port";
      return;
    }
    p.wire(gout, gin);
    const int sink = p.addSink({kSize, kSize});
    p.wire(p.imageOut(raster, 0), p.sinkInput(sink));

    if(!p.create(be))
    {
      r.backend = p.backend();
      r.skipped = p.skipped();
      r.skip_reason = p.skipReason();
      r.error = p.error();
      return;
    }
    r.backend = p.backend();

    setControl(*p.isf(csf), kPoisonPort, ossia::value{poison});
    setControl(*p.isf(csf), kCountPort, ossia::value{kCountA});
    p.render(kFrames);
    r.at3 = p.readback(sink);

    setControl(*p.isf(csf), kCountPort, ossia::value{kCountB});
    p.render(kFrames);
    r.at6 = p.readback(sink);

    // Parse the LAST caps line (the session's own RenderList; earlier probe
    // RenderStates may have printed one too).
    static const std::regex caps_re(
        "RHI indirect caps: drawIndirect=([01]) multi=([01]) count=([01]) "
        "dispatchIndirect=([01])");
    for(std::sregex_iterator
            it(capture.text.begin(), capture.text.end(), caps_re),
        end;
        it != end; ++it)
    {
      r.capsParsed = true;
      r.capsDrawIndirect = (*it)[1] == "1";
      r.capsMulti = (*it)[2] == "1";
      r.capsCount = (*it)[3] == "1";
      r.capsDispatch = (*it)[4] == "1";
    }
    if(capture.text.find("CSF indirect dispatch: gpu") != std::string::npos)
      r.dispatchLog = "gpu";
    else if(
        capture.text.find("CSF indirect dispatch: cpu-fallback")
        != std::string::npos)
      r.dispatchLog = "cpu-fallback";
  });

  for(const char* s : kSwitches)
    qunsetenv(s);
  return r;
}

/// Assert one frame: real strips [0, count) present with their identity,
/// real strips [count, 8) absent, and the poison area (columns 32..63)
/// showing exactly the dead slots' sentinel quads when poisonVisible -- or
/// nothing at all when not.
void check_frame(
    const ReadbackImage& img, int count, bool poisonVisible, const char* which)
{
  INFO("frame " << which << " count=" << count
                << " poisonVisible=" << poisonVisible);
  REQUIRE(img.valid());
  REQUIRE(img.width == kSize);
  REQUIRE(img.height == kSize);

  const int rows[3] = {8, kSize / 2, kSize - 8};
  for(int q = 0; q < kSlots; ++q)
  {
    for(int dx = 1; dx <= 2; ++dx)
    {
      const int x = q * kStripPx + dx;
      for(int y : rows)
      {
        const auto px = img.at(x, y);
        INFO(
            "real strip " << q << " px(" << x << "," << y << ")=("
                          << int(px[0]) << "," << int(px[1]) << ","
                          << int(px[2]) << "," << int(px[3]) << ")");
        if(q < count)
          CHECK(near(px, real_px(q), kTol));
        else
          CHECK(int(px[0]) < 255 - 4 * kTol); // no R marker: not drawn
      }
    }
  }
  for(int j = 0; j < kSlots; ++j)
  {
    for(int dx = 1; dx <= 2; ++dx)
    {
      const int x = (kSlots + j) * kStripPx + dx;
      for(int y : rows)
      {
        const auto px = img.at(x, y);
        INFO(
            "poison strip " << j << " px(" << x << "," << y << ")=("
                            << int(px[0]) << "," << int(px[1]) << ","
                            << int(px[2]) << "," << int(px[3]) << ")");
        // Dead slots are [count, kSlots): only those carry a poison draw.
        if(poisonVisible && j >= count)
          CHECK(near(px, kPoisonPx, kTol));
        else
          CHECK(int(px[2]) < 255 - 4 * kTol); // no B sentinel: not drawn
      }
    }
  }
}

void check_session(const LadderResult& r, bool poisonVisible)
{
  REQUIRE(r.error.empty());
  REQUIRE(r.capsParsed);
  check_frame(r.at3, kCountA, poisonVisible, "count=3");
  check_frame(r.at6, kCountB, poisonVisible, "count=6");
}
} // namespace

TEST_CASE(
    "indirect draw fallback ladder: every rung paints the same picture and "
    "each kill switch provably lands",
    "[gfx][l3][raster][csf][indirect][fallback]")
{
  const auto backend = GENERATE(from_range(platform_backends()));

  // S1: no switches -- the best rung this backend/Qt offers.
  const LadderResult s1 = run_session(backend, {}, /*poison=*/1);
  if(s1.skipped)
    SKIP(s1.backend + ": " + s1.skip_reason);
  if(const char* why = compute_shader_skip_reason(backend))
    SKIP(why);
  INFO("backend=" << s1.backend);

  // Whether the count rung (R1) actually engages: the caps line carries the
  // QRhi feature; drawIndirectCountUsable() additionally declines Metal
  // until UsesIndirectDraws / no-texture / NoTransientBacking are plumbed
  // (RenderState.hpp) -- mirror that here.
  const bool metal = backend == score::gfx::Metal;
  const bool countRung = s1.capsCount == 1 && !metal;
  {
    INFO(
        "S1 caps: drawIndirect=" << s1.capsDrawIndirect << " multi="
                                 << s1.capsMulti << " count=" << s1.capsCount
                                 << " dispatchIndirect=" << s1.capsDispatch
                                 << " countRung=" << countRung);
    // Poison appears exactly when a capacity rung draws dead slots: an
    // indirect-capable backend without the count rung.
    const bool poisonVisible = s1.capsDrawIndirect == 1 && !countRung;
    check_session(s1, poisonVisible);
    // The INDIRECT pass must have dispatched, on the path the caps promise.
    REQUIRE(!s1.dispatchLog.empty());
    CHECK(s1.dispatchLog == (s1.capsDispatch == 1 ? "gpu" : "cpu-fallback"));
  }

  // S2: count rung forced off -- the capacity multi-draw rung (R2).
  {
    const LadderResult s2
        = run_session(backend, {"SCORE_GFX_NO_GPU_INDIRECT_COUNT"}, 1);
    INFO("S2 (NO_GPU_INDIRECT_COUNT)");
    REQUIRE(s2.error.empty());
    REQUIRE(s2.capsParsed);
    REQUIRE(s2.capsCount == 0); // the switch landed
    check_session(s2, /*poisonVisible=*/s2.capsDrawIndirect == 1);
  }

  // S3: additionally no multi -- the per-command loop rung (R3).
  {
    const LadderResult s3 = run_session(
        backend,
        {"SCORE_GFX_NO_GPU_INDIRECT_COUNT", "SCORE_GFX_NO_GPU_INDIRECT_MULTI"},
        1);
    INFO("S3 (NO_GPU_INDIRECT_COUNT + NO_GPU_INDIRECT_MULTI)");
    REQUIRE(s3.error.empty());
    REQUIRE(s3.capsParsed);
    REQUIRE(s3.capsCount == 0);
    REQUIRE(s3.capsMulti == 0);
    check_session(s3, /*poisonVisible=*/s3.capsDrawIndirect == 1);
  }

  // S4: no GPU indirect at all -- the CPU readback rung (R4). Poison stays ON
  // and must NOT appear: the readback clamps the command list to the
  // GPU-written count, which is exactly the contract under test.
  {
    const LadderResult s4
        = run_session(backend, {"SCORE_GFX_NO_GPU_INDIRECT"}, 1);
    INFO("S4 (NO_GPU_INDIRECT)");
    REQUIRE(s4.error.empty());
    REQUIRE(s4.capsParsed);
    REQUIRE(s4.capsDrawIndirect == 0);
    REQUIRE(s4.capsCount == 0);
    check_session(s4, /*poisonVisible=*/false);
  }

  // S5: indirect dispatch forced onto the CPU ceiling (D2); pixels must not
  // move. Poison off -- this session is about the dispatch half.
  {
    const LadderResult s5
        = run_session(backend, {"SCORE_GFX_NO_GPU_DISPATCH_INDIRECT"}, 0);
    INFO("S5 (NO_GPU_DISPATCH_INDIRECT)");
    REQUIRE(s5.error.empty());
    REQUIRE(s5.capsParsed);
    REQUIRE(s5.capsDispatch == 0);
    REQUIRE(s5.dispatchLog == "cpu-fallback");
    check_session(s5, /*poisonVisible=*/false);
    // Equivalence against S1's frames where the expected pictures coincide:
    // when S1 had no poison on screen, its frames and S5's must agree.
    if(!(s1.capsDrawIndirect == 1 && !countRung))
    {
      CHECK(max_channel_diff(s1.at6, s5.at6) <= kTol);
      CHECK(max_channel_diff(s1.at3, s5.at3) <= kTol);
    }
  }
}
