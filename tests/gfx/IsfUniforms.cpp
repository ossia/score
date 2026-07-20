// =============================================================================
// L3 ISF feature surface — TIME / BUILT-IN UNIFORMS and long numeric input.
//
// isf-time-uniforms.fs lays out six horizontal rows, one per built-in uniform
// (TIME, TIMEDELTA, PROGRESS, FRAMEINDEX, RENDERSIZE, DATE). We assert:
//   * the RENDERSIZE row is the analytic value for a 64x64 target
//     (RENDERSIZE/2048 -> (8,8,127)), on every backend; and
//   * the FRAMEINDEX row CHANGES between a short (3) and a long (40) frame run —
//     `frameIndex` advances once per rendered frame — proving the per-frame
//     uniform is delivered.
//
// Determinism note: the fixture feeds `date` in flicks, so ISF TIME ≈ 0; the
// per-frame advancing uniform available here is FRAMEINDEX (incremented by the
// renderer each frame), which is what the change assertion keys on.
//
// isf-long-numeric.fs draws `inCount` horizontal colour bars; with no control
// injection the ISF DEFAULT (4) is used, so we assert a banded, multi-colour,
// non-degenerate output that agrees across backends.
//
// Run: DISPLAY=:0 ctest -R gfx --output-on-failure
// =============================================================================
#include "IsfTestCommon.hpp"

#include <set>

using namespace score::test::gfx;
using namespace score::test::gfx::isf;

namespace
{
// Row centres (readback y) for isf-time-uniforms: RENDERSIZE row and FRAMEINDEX
// row (measured on this box; the sink readback is Y-corrected on all backends).
constexpr int kRenderSizeY = 16;
constexpr int kFrameIndexY = 27;
}

TEST_CASE("isf-time-uniforms exposes RENDERSIZE and advances FRAMEINDEX", "[gfx][l3][isf][uniforms]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  struct Out
  {
    bool skipped = false;
    std::string skip_reason, backend, error;
    std::array<uint8_t, 4> rsize{}, frameShort{}, frameLong{};
  } out;

  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    auto rShort = score::test::gfx::render_isf_chain(
        backend, {corpus("isf-time-uniforms.fs")}, {64, 64}, 3);
    auto rLong = score::test::gfx::render_isf_chain(
        backend, {corpus("isf-time-uniforms.fs")}, {64, 64}, 40);
    out.skipped = rShort.skipped;
    out.skip_reason = rShort.skip_reason;
    out.backend = rShort.backend;
    out.error = rShort.error.empty() ? rLong.error : rShort.error;
    if(!rShort.outputs.empty() && rShort.outputs[0].valid())
    {
      out.rsize = rShort.outputs[0].at(32, kRenderSizeY);
      out.frameShort = rShort.outputs[0].at(32, kFrameIndexY);
    }
    if(!rLong.outputs.empty() && rLong.outputs[0].valid())
      out.frameLong = rLong.outputs[0].at(32, kFrameIndexY);
  });

  if(out.skipped)
    SKIP(out.backend + ": " + out.skip_reason);
  INFO("backend=" << out.backend);
  REQUIRE(out.error.empty());

  // RENDERSIZE row: 64/2048 -> 0.03125 -> 8 (R,G); 0.5 -> 127 (B). Analytic.
  INFO("RENDERSIZE row = (" << (int)out.rsize[0] << "," << (int)out.rsize[1] << ","
                            << (int)out.rsize[2] << ")");
  CHECK(near(out.rsize, {8, 8, 127, 255}, 6));

  // FRAMEINDEX row: the cycling-hue colour must differ between 3 and 40 frames.
  INFO("FRAMEINDEX row short=(" << (int)out.frameShort[0] << "," << (int)out.frameShort[1]
                               << "," << (int)out.frameShort[2] << ") long=("
                               << (int)out.frameLong[0] << "," << (int)out.frameLong[1]
                               << "," << (int)out.frameLong[2] << ")");
  CHECK(!near(out.frameShort, out.frameLong, 24));
}

TEST_CASE("isf-time-uniforms agrees across backends", "[gfx][l3][isf][uniforms]")
{
  const auto shots = render_all({corpus("isf-time-uniforms.fs")}, {64, 64}, 8);
  std::vector<ReadbackImage> got;
  for(const auto& s : shots)
    if(!s.result.skipped && s.result.error.empty() && !s.result.outputs.empty()
       && s.result.outputs[0].valid())
      got.push_back(s.result.outputs[0]);
  if(got.size() < 2)
    SKIP("need >=2 live backends to compare");
  for(std::size_t i = 1; i < got.size(); ++i)
  {
    const int d = max_channel_diff(got[0], got[i]);
    INFO("max channel diff vs backend0 = " << d);
    CHECK(d <= 4);
  }
}

// -----------------------------------------------------------------------------
// isf-long-numeric: `inCount` (long, DEFAULT 4) horizontal colour bars.
// -----------------------------------------------------------------------------
TEST_CASE("isf-long-numeric renders banded bars from the default count", "[gfx][l3][isf][uniforms]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  const IsfResult r = render(backend, {corpus("isf-long-numeric.fs")});
  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);
  INFO("backend=" << r.backend);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() == 1);
  const auto& img = r.outputs[0];
  REQUIRE(img.valid());

  CHECK(non_degenerate(img));

  // Sample a vertical strip: with 4 bars we must see several DISTINCT band
  // colours (quantise to 32-levels to ignore rounding).
  std::set<int> bands;
  for(int y = 4; y < img.height - 4; y += 4)
  {
    const auto p = img.at(32, y);
    // ignore the dark inter-bar gaps
    if(p[0] + p[1] + p[2] < 40)
      continue;
    const int key = (p[0] / 32) * 1024 + (p[1] / 32) * 32 + (p[2] / 32);
    bands.insert(key);
  }
  INFO("distinct band colours = " << bands.size());
  CHECK(bands.size() >= 3);
}

TEST_CASE("isf-long-numeric agrees across backends", "[gfx][l3][isf][uniforms]")
{
  const auto shots = render_all({corpus("isf-long-numeric.fs")});
  std::vector<ReadbackImage> got;
  for(const auto& s : shots)
    if(!s.result.skipped && s.result.error.empty() && !s.result.outputs.empty()
       && s.result.outputs[0].valid())
      got.push_back(s.result.outputs[0]);
  if(got.size() < 2)
    SKIP("need >=2 live backends to compare");
  for(std::size_t i = 1; i < got.size(); ++i)
  {
    const int d = max_channel_diff(got[0], got[i]);
    INFO("max channel diff vs backend0 = " << d);
    CHECK(d <= 4);
  }
}
