// The shader previews (Gfx/Widgets/RhiPreviewWidget.cpp) build a fresh
// BackgroundNode — and therefore a fresh RenderState — on every selection, and
// tear the previous one down first. On Vulkan that used to mean a
// vkCreateDevice (150-210 ms) plus a vkDestroyDevice (80-140 ms) per click, all
// on the GUI thread.
//
// SharedVulkanDeviceCache fixes that by keeping one imported VkDevice for the
// process lifetime. The subtle part is the ordering: because selection destroys
// before it creates, a cache that freed the device at refcount zero would drop
// to zero and back to one across every transition and save nothing.
//
// WHAT THIS TEST USED TO PIN, AND WHY THAT WAS NOT ENOUGH. It asserted a count
// -- `createdDeviceCount() <= 1` -- and merely PRINTED the per-selection cost.
// A count of one is also what you get when the cache is not used at all:
// `SCORE_GFX_NO_VKDEVICE_CACHE=1` sends every selection down
// createSharedVulkanDevice() and leaves the counter at ZERO, so the assertion
// passed vacuously. Measured on this machine, the binary printed 194/182/182/177
// ms per selection under that variable and passed all 22 assertions -- i.e. the
// test could not fail on the exact regression it exists to guard, which is a
// latency regression and not a count.
//
// So the count is kept and a MEASUREMENT is added, in the only form that
// survives a machine of unknown speed: the same five selections are run twice
// in one process, once with SharedDeviceMode::Owned (a device created and
// destroyed per selection, which is the pre-fix behaviour and is still what
// every non-preview caller of createRenderState relies on) and once Cached.
// Cached must be dramatically cheaper than Owned on the same box in the same
// run. The absolute numbers are printed for the record against the project's
// <50 ms interaction standard, but the ASSERTION is the ratio, because an
// absolute millisecond budget on a CI runner of unknown load is a flake.

#include <Gfx/Graph/VulkanVideoDevice.hpp>

#include <score_test/Gfx.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <vector>

using namespace score::test::gfx;

namespace
{
constexpr int kSelections = 5;

QString corpus(const char* file)
{
  return QString{GFX_TEST_CORPUS_DIR "/"} + file;
}

/// One "preview selection": the previous render graph and its BackgroundNode
/// are gone by the time render_isf_chain returns, and the next call builds a
/// new one — the exact create-after-destroy order the library preview uses.
struct PreviewShot
{
  IsfResult result;
  double ms{};
};

std::vector<PreviewShot>
selectSequentially(score::gfx::SharedDeviceMode mode, const char* const* shaders)
{
  std::vector<PreviewShot> out;
  for(int i = 0; i < kSelections; ++i)
  {
    const auto t0 = std::chrono::steady_clock::now();
    auto r = render_isf_chain(
        score::gfx::Vulkan, {corpus(shaders[i])}, QSize{64, 64}, 3, mode);
    const auto t1 = std::chrono::steady_clock::now();
    out.push_back(
        {std::move(r),
         std::chrono::duration<double, std::milli>(t1 - t0).count()});
  }
  return out;
}

/// Median of the STEADY-STATE selections, i.e. everything but the first: the
/// cold one pays for driver load, shader compilation and the first pipeline
/// cache and is not what the user feels when clicking down a shader list.
double steadyStateMedianMs(const std::vector<PreviewShot>& runs)
{
  std::vector<double> ms;
  for(std::size_t i = 1; i < runs.size(); ++i)
    ms.push_back(runs[i].ms);
  if(ms.empty())
    return 0.;
  std::sort(ms.begin(), ms.end());
  return ms[ms.size() / 2];
}

/// True if the readback is not one constant colour, i.e. something was drawn.
bool nonDegenerate(const ReadbackImage& img)
{
  if(!img.valid())
    return false;
  const auto first = img.at(2, 2);
  for(int y = 2; y < img.height - 2; y += 2)
    for(int x = 2; x < img.width - 2; x += 2)
      if(!near(img.at(x, y), first, 6))
        return true;
  return false;
}

const char* const kShaders[kSelections] = {
    "isf-gradient-x.fs", "isf-gradient-y.fs", "isf-gradient-x.fs",
    "isf-gradient-y.fs", "isf-gradient-x.fs"};
}

TEST_CASE("Cached previews create one VkDevice for the whole session", "[gfx]")
{
#if !QT_HAS_VULKAN || !defined(VK_KHR_video_decode_queue)
  SUCCEED("built without the Vulkan shared-device path");
#else
  std::vector<PreviewShot> owned, cached;
  int devicesCreated = -1;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    // Owned FIRST so it absorbs the cold cost (driver load, shader compile,
    // pipeline cache) instead of handing it to the run we want to look good.
    owned = selectSequentially(score::gfx::SharedDeviceMode::Owned, kShaders);
    cached = selectSequentially(score::gfx::SharedDeviceMode::Cached, kShaders);
    devicesCreated = score::gfx::sharedVulkanDeviceCache().createdDeviceCount();
  });

  REQUIRE(owned.size() == std::size_t(kSelections));
  REQUIRE(cached.size() == std::size_t(kSelections));
  if(owned.front().result.skipped)
  {
    SUCCEED("Vulkan unavailable here: " + owned.front().result.skip_reason);
    return;
  }

  const auto check = [](const char* label, const std::vector<PreviewShot>& runs) {
    for(int i = 0; i < kSelections; ++i)
    {
      // Printed, not just INFO'd: the per-selection cost is the number this
      // change exists to move, and it is worth seeing on a passing run.
      std::fprintf(
          stderr, "PREVIEW-SELECTION %s %d: %.1f ms\n", label, i, runs[i].ms);
      INFO(
          label << " selection " << i << " took " << runs[i].ms
                << " ms, error=" << runs[i].result.error);
      REQUIRE(runs[i].result.error.empty());
      REQUIRE(runs[i].result.outputs.size() == 1u);
      // A preview that is fast because it draws nothing is a regression.
      REQUIRE(nonDegenerate(runs[i].result.outputs[0]));
    }
  };
  check("owned", owned);
  check("cached", cached);

  // Zero means the box has no video-decode queue and the caller fell back to a
  // QRhi-owned device — that is the documented fallback, not a failure;
  // anything above one means the cache is not caching.
  INFO("devices created by the cache: " << devicesCreated);
  REQUIRE(devicesCreated <= 1);

  const double ownedMs = steadyStateMedianMs(owned);
  const double cachedMs = steadyStateMedianMs(cached);
  std::fprintf(
      stderr,
      "PREVIEW-STEADY-STATE MEDIAN: owned %.1f ms, cached %.1f ms "
      "(project standard: < 50 ms per selection)\n",
      ownedMs, cachedMs);

  // THE ASSERTION THIS TEST EXISTS FOR. Selecting a shader must not pay for a
  // vkCreateDevice + vkDestroyDevice pair.
  //
  // The gate is the OWNED run, not `devicesCreated`. A first attempt gated on
  // `devicesCreated == 0` and was vacuous again for exactly the same reason as
  // the original: with SCORE_GFX_NO_VKDEVICE_CACHE=1 the counter stays at zero,
  // so the bypass switched the assertion off instead of tripping it (measured:
  // owned 187.2 / cached 192.9 ms, all 39 assertions still green). What
  // actually says "the expensive path is in play on this box" is the owned
  // run's own cost: a machine with no video-decode queue takes the plain
  // QRhi::create fallback and is fast in BOTH halves, so there is no stutter
  // to assert about. Above the standard, the device pair is being paid and the
  // cached half has to avoid it.
  //
  // The comparison itself is a RATIO against a baseline measured moments
  // earlier on the same box, so it holds on a runner of any speed; 0.5 is a
  // wide margin around the measured 0.10 (cached 20.1 ms against owned 196.9
  // ms on the reference machine). The 50 ms in the gate is the project's
  // interaction standard used as a "is there anything to measure" threshold,
  // never as the pass criterion -- a millisecond budget on a CI runner of
  // unknown load would be a flake.
  constexpr double kInteractionStandardMs = 50.;
  INFO(
      "steady-state median: owned " << ownedMs << " ms, cached " << cachedMs
                                    << " ms; devices created " << devicesCreated);
  REQUIRE(ownedMs > 0.);
  if(ownedMs <= kInteractionStandardMs)
  {
    SUCCEED(
        "a preview costs no device pair on this box; nothing to accelerate");
    return;
  }
  CHECK(cachedMs < 0.5 * ownedMs);
#endif
}

// The library preview and the node-selection preview are separate panels and
// can be on screen together, so the refcount really does go above one and two
// QRhis end up importing the same VkDevice. An MRT shader reproduces that
// without a GUI: render_isf_chain gives every image output its own
// BackgroundNode, and they are alive at the same time.
TEST_CASE("Simultaneous cached previews share one device", "[gfx]")
{
#if !QT_HAS_VULKAN || !defined(VK_KHR_video_decode_queue)
  SUCCEED("built without the Vulkan shared-device path");
#else
  IsfResult r;
  int devicesCreated = -1;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    r = render_isf_chain(
        score::gfx::Vulkan, {corpus("isf-mrt-gradient-y.fs")}, QSize{64, 64}, 3,
        score::gfx::SharedDeviceMode::Cached);
    devicesCreated = score::gfx::sharedVulkanDeviceCache().createdDeviceCount();
  });

  if(r.skipped)
  {
    SUCCEED("Vulkan unavailable here: " + r.skip_reason);
    return;
  }

  INFO("error=" << r.error << " devices=" << devicesCreated);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() == 2u);
  for(const auto& img : r.outputs)
    REQUIRE(nonDegenerate(img));
  REQUIRE(devicesCreated <= 1);
#endif
}
