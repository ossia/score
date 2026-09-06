// =============================================================================
// GPU-DRIVEN DYNAMIC LIGHTS: dispatchIndirect + drawIndirectCount, ENGAGED.
//
// Registration: score_add_gfx_test(gpu_driven_lights GfxGpuDrivenLights.cpp)
//
// The stage-2 exemplar pipeline (corpus/syn-gpu-lights.cs, consumer
// corpus/syn-vertex-color.{vs,fs}; documentation copies live in
// csf-examples/gpu-driven/): a pool of 16 light slots on a 4x4 grid. A
// one-thread "light manager" pass writes, ON THE GPU, the indirect dispatch
// arguments for the per-light pass, the draw count, and the dead-slot filler;
// the per-light pass is dispatched INDIRECTLY (one workgroup per alive light)
// and writes each light's quad + draw command; the draw consumes the commands
// through drawIndirectCount. The CPU never learns the light count or their
// placement — both move at spec level via the 'alive' and 'tick' controls.
//
// Unlike GfxIndirectFallbackLadder (which walks every fallback rung), this
// test PROVES THE TOP RUNGS ENGAGED and fails or SKIPS otherwise:
//  * REQUIRE the captured caps line reports count=1 dispatchIndirect=1 (on
//    backends where Qt reports the features; otherwise SKIP with the reason
//    stated — never a silent pass on the wrong path);
//  * REQUIRE the CSF pass logged "indirect dispatch: gpu";
//  * dead command slots are POISONED with sentinel-blue draws: any path that
//    draws the full capacity instead of the GPU count paints blue and fails.
//    (The CPU readback rung would also suppress poison by clamping, but the
//    caps line REQUIREs drawIndirect=1, under which the draw code selects the
//    count rung — rung selection itself is pinned by the ladder test.)
//
// Oracle (64x64, all coordinates exact powers of two): alive light in cell c
// = an 8x8 px quad centered in cell (c%4, c/4), reading back {255, 16*c, 0,
// 255}; poison reads {0, 0, 255, 255}; empty cells stay unlit. Three phases
// in ONE session: (alive=5, tick=0, poison=1), (alive=11, tick=0, poison=1),
// (alive=5, tick=7, poison=0) — the count moves, then the placement moves,
// with no CPU-side geometry change whatsoever.
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
constexpr int kSlots = 16;
constexpr int kTol = 2;
constexpr int kAlivePort = 0;
constexpr int kTickPort = 1;
constexpr int kPoisonPort = 2;
constexpr int kFrames = 3;

std::array<uint8_t, 4> light_px(int cell)
{
  return {255, uint8_t(16 * cell), 0, 255};
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

struct LightsResult
{
  bool skipped = false;
  std::string skip_reason;
  std::string error;
  std::string backend;
  bool capsParsed = false;
  int capsDrawIndirect = -1, capsMulti = -1, capsCount = -1, capsDispatch = -1;
  std::string dispatchLog;
  ReadbackImage phase1, phase2, phase3;
};

LightsResult run_lights(score::gfx::GraphicsApi be)
{
  LightsResult r;
  r.backend = backend_name(be);
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    LogCapture capture;

    GfxPipeline p;
    const int csf = p.addIsf(corpus("syn-gpu-lights.cs"));
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
      r.error = "geometry ports missing";
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

    setControl(*p.isf(csf), kPoisonPort, ossia::value{1});
    setControl(*p.isf(csf), kAlivePort, ossia::value{5});
    setControl(*p.isf(csf), kTickPort, ossia::value{0});
    p.render(kFrames);
    r.phase1 = p.readback(sink);

    setControl(*p.isf(csf), kAlivePort, ossia::value{11});
    p.render(kFrames);
    r.phase2 = p.readback(sink);

    setControl(*p.isf(csf), kAlivePort, ossia::value{5});
    setControl(*p.isf(csf), kTickPort, ossia::value{7});
    setControl(*p.isf(csf), kPoisonPort, ossia::value{0});
    p.render(kFrames);
    r.phase3 = p.readback(sink);

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
  return r;
}

/// Sample points inside cell c's 8x8 centered quad. NDC (+1 = top after the
/// fixture's Y correction): NDC cell row cy occupies screen rows
/// [52 - 16*cy, 60 - 16*cy).
struct CellPx
{
  int cols[2];
  int rows[2];
};
CellPx cell_px(int cell)
{
  const int cx = cell % 4, cy = cell / 4;
  return {{16 * cx + 6, 16 * cx + 9}, {54 - 16 * cy, 57 - 16 * cy}};
}

/// Assert one frame: exactly the cells in `lit` carry their light color, no
/// cell carries poison blue, every other cell is unlit.
void check_frame(const ReadbackImage& img, const std::array<bool, 16>& lit,
                 const char* which)
{
  INFO("frame " << which);
  REQUIRE(img.valid());
  REQUIRE(img.width == kSize);
  REQUIRE(img.height == kSize);
  for(int c = 0; c < kSlots; ++c)
  {
    const CellPx cp = cell_px(c);
    for(int x : cp.cols)
      for(int y : cp.rows)
      {
        const auto px = img.at(x, y);
        INFO(
            "cell " << c << " px(" << x << "," << y << ")=(" << int(px[0])
                    << "," << int(px[1]) << "," << int(px[2]) << ","
                    << int(px[3]) << ")");
        // Poison is never acceptable in this test: it means something drew
        // past the GPU-written count.
        CHECK(int(px[2]) < 255 - 4 * kTol);
        if(lit[c])
          CHECK(near(px, light_px(c), kTol));
        else
          CHECK(int(px[0]) < 255 - 4 * kTol);
      }
  }
}
} // namespace

TEST_CASE(
    "GPU-driven dynamic lights: indirect dispatch and GPU draw count engaged",
    "[gfx][l3][raster][csf][indirect][lights]")
{
  const auto backend = GENERATE(from_range(platform_backends()));

  const LightsResult r = run_lights(backend);
  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);
  if(const char* why = compute_shader_skip_reason(backend))
    SKIP(why);
  INFO("backend=" << r.backend);
  REQUIRE(r.error.empty());
  REQUIRE(r.capsParsed);
  INFO(
      "caps: drawIndirect=" << r.capsDrawIndirect << " multi=" << r.capsMulti
                            << " count=" << r.capsCount << " dispatchIndirect="
                            << r.capsDispatch << " dispatchLog="
                            << r.dispatchLog);

  // Engagement gate: this test exists to see the TOP rungs run. Where the
  // backend/Qt does not offer them, the ladder test already pins the
  // fallbacks — skip with the reason on record, never pass on the wrong path.
  if(r.capsCount != 1 || r.capsDispatch != 1)
    SKIP(
        r.backend
        + ": QRhi::DrawIndirectCount/DispatchIndirect not reported (caps line)"
          " — top rungs cannot engage on this backend/Qt tier");
  if(backend == score::gfx::Metal)
    SKIP("Metal: count rung declined by drawIndirectCountUsable() until "
         "UsesIndirectDraws/no-texture/NoTransientBacking are plumbed");

  // The INDIRECT pass must have gone through cb.dispatchIndirect.
  REQUIRE(r.dispatchLog == "gpu");

  // Phase 1: 5 lights, cells 0..4, dead slots poisoned — poison must not
  // appear anywhere (the count came from the GPU buffer).
  std::array<bool, 16> lit{};
  for(int c = 0; c < 5; ++c)
    lit[c] = true;
  check_frame(r.phase1, lit, "alive=5 tick=0 poison=1");

  // Phase 2: the GPU count moved to 11 within the same session.
  lit = {};
  for(int c = 0; c < 11; ++c)
    lit[c] = true;
  check_frame(r.phase2, lit, "alive=11 tick=0 poison=1");

  // Phase 3: same count, placement moved — cells (7+i)%16, i<5.
  lit = {};
  for(int i = 0; i < 5; ++i)
    lit[(7 + i) % 16] = true;
  check_frame(r.phase3, lit, "alive=5 tick=7 poison=0");
}
