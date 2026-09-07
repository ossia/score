// =============================================================================
// A DEAD COMMAND SLOT MUST NOT BECOME A DRAW CALL.
//
// Registration: score_add_gfx_test(zero_count_draw GfxZeroCountDraw.cpp)
//
// RenderState::Caps states the producer contract that makes the indirect
// fallback ladder's rungs equivalent: command slots beyond the written count
// stay ZEROED. The capacity rungs (R2/R3) rely on it -- they issue the full
// slot capacity and let the GPU discard the zeros.
//
// The CPU-readback rung (R4) reads those same slots back and replays them as
// explicit API calls. It clamps with the GPU-written count when one exists --
// but the count buffer is OPTIONAL: it appears only when the producer declares
// an "_indirect_draw_count" auxiliary (CustomMesh.cpp:180). A producer that
// declares INDIRECT without DRAW_COUNT is a supported configuration, and there
// the CPU rung has nothing to clamp with, so every dead slot arrives at the
// draw loop as an all-zero command.
//
// Vulkan, D3D and GL treat drawIndexed(0) as a legal no-op. Metal's API
// validation layer treats it as a programming error and ABORTS the process
// ("indexCount(0) must be non-zero"). The rungs are therefore only equivalent
// if the CPU rung skips what the GPU rung would have discarded.
//
// The fixture (corpus/syn-indirect-nocount.cs) is the ladder fixture with its
// DRAW_COUNT removed: 8 command slots, all zeroed by pass 0, slots [0, count)
// overwritten with real strip draws by pass 1, no count published.
//
// POSITIVE CONTROLS -- because on every backend that is not Metal the pixels
// are IDENTICAL whether the dead slots were skipped or issued as no-op draws,
// so a pixel oracle alone cannot tell a working filter from a deleted one:
//  * the caps line proves SCORE_GFX_NO_GPU_INDIRECT really landed and the
//    session rendered on the CPU rung rather than an indirect one;
//  * Mesh.cpp's zero-count skip COUNTER, bracketed around the session: it must
//    advance by exactly the number of dead slots and by nothing at all when
//    every slot is live, so deleting the filter fails here on Linux instead of
//    only aborting on macOS. (This started as a warn-once log line, which does
//    not survive a Catch2 process running several backends in sequence: the
//    first session consumes the only line and every later one reads false.)
// The pixel oracle then proves the filter did not over-reach and drop draws
// that should have painted.
// =============================================================================
#include "IsfTestCommon.hpp"

#include <Gfx/Graph/Mesh.hpp>

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
constexpr int kSlots = 8;
constexpr int kCountPort = 0;
constexpr int kFrames = 3;

std::array<uint8_t, 4> real_px(int q)
{
  return {255, uint8_t(q), 0, 255};
}

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

struct Result
{
  bool skipped{};
  std::string backend, skip_reason, error;
  bool capsParsed{};
  int capsDrawIndirect{-1};
  uint64_t skippedDelta{};
  ReadbackImage frame;
};

Result run_session(score::gfx::GraphicsApi be, int count)
{
  Result r;
  qputenv("SCORE_GFX_NO_GPU_INDIRECT", "1");
  const uint64_t skippedBefore = score::gfx::zeroCountSlotsSkippedTotal();
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    LogCapture capture;

    GfxPipeline p;
    const int csf = p.addIsf(corpus("syn-indirect-nocount.cs"));
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

    setControl(*p.isf(csf), kCountPort, ossia::value{count});
    p.render(kFrames);
    r.frame = p.readback(sink);

    static const std::regex caps_re("RHI indirect caps: drawIndirect=([01])");
    for(std::sregex_iterator it(capture.text.begin(), capture.text.end(), caps_re),
        end;
        it != end; ++it)
    {
      r.capsParsed = true;
      r.capsDrawIndirect = (*it)[1] == "1";
    }
    r.skippedDelta = score::gfx::zeroCountSlotsSkippedTotal() - skippedBefore;
  });
  qunsetenv("SCORE_GFX_NO_GPU_INDIRECT");
  return r;
}
} // namespace

TEST_CASE(
    "gfx zero-count draw: the CPU rung skips the producer's zeroed dead slots",
    "[gfx][indirect][zero-count]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  const int count = GENERATE(3, 8);
  const Result r = run_session(be, count);
  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);
  if(const char* why = compute_shader_skip_reason(be))
    SKIP(why);

  INFO("backend=" << r.backend << " count=" << count);
  REQUIRE(r.error.empty());

  // Positive control 1: the kill switch landed and this really is the CPU rung.
  REQUIRE(r.capsParsed);
  CHECK(r.capsDrawIndirect == 0);

  // Positive control 2: the filter actually fired, and fired on exactly the
  // dead slots. kFrames frames are rendered, each replaying the full capacity,
  // so the expected delta is (dead slots x frames). At count=8 every slot is
  // live and the counter must not move at all -- which also proves the counter
  // tracks real skips rather than being bumped unconditionally.
  const uint64_t deadPerFrame = kSlots - count;
  CHECK(r.skippedDelta == deadPerFrame * kFrames);

  // And the filter did not over-reach: live slots still paint, dead ones and
  // the poison-quad columns (32..63, never commanded here) stay clear.
  REQUIRE(r.frame.valid());
  REQUIRE(r.frame.width == kSize);
  REQUIRE(r.frame.height == kSize);
  const int rows[3] = {8, kSize / 2, kSize - 8};
  for(int q = 0; q < kSlots; ++q)
  {
    for(int dx = 1; dx <= 2; ++dx)
    {
      const int x = q * kStripPx + dx;
      for(int y : rows)
      {
        const auto px = r.frame.at(x, y);
        INFO(
            "strip " << q << " px(" << x << "," << y << ")=(" << int(px[0]) << ","
                     << int(px[1]) << "," << int(px[2]) << "," << int(px[3]) << ")");
        if(q < count)
          CHECK(near(px, real_px(q), kTol));
        else
          CHECK(int(px[0]) < 255 - 4 * kTol);
      }
    }
  }
  for(int x = kSlots * kStripPx + 1; x < kSize; x += kStripPx)
  {
    for(int y : rows)
    {
      const auto px = r.frame.at(x, y);
      INFO("beyond-slots px(" << x << "," << y << ")=(" << int(px[0]) << ","
                              << int(px[1]) << "," << int(px[2]) << ")");
      CHECK(int(px[2]) < 255 - 4 * kTol);
    }
  }
}
