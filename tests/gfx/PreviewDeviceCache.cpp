// The shader previews (Gfx/Widgets/RhiPreviewWidget.cpp) build a fresh
// BackgroundNode — and therefore a fresh RenderState — on every selection, and
// tear the previous one down first. On Vulkan that used to mean a
// vkCreateDevice (150-210 ms) plus a vkDestroyDevice (80-140 ms) per click, all
// on the GUI thread.
//
// SharedVulkanDeviceCache fixes that by keeping one imported VkDevice for the
// process lifetime. The subtle part is the ordering: because selection destroys
// before it creates, a cache that freed the device at refcount zero would drop
// to zero and back to one across every transition and save nothing. What this
// test pins is therefore not a timing but a count — one device, no matter how
// many previews are opened — plus the fact that each of those previews still
// renders real content.
//
// The Owned half is the control: the same loop through the same code with
// SharedDeviceMode::Owned must still create one device per selection, because
// every non-preview caller of createRenderState relies on that.

#include <Gfx/Graph/VulkanVideoDevice.hpp>

#include <score_test/Gfx.hpp>

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdio>

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
  std::vector<PreviewShot> runs;
  int devicesCreated = -1;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    runs = selectSequentially(score::gfx::SharedDeviceMode::Cached, kShaders);
    devicesCreated = score::gfx::sharedVulkanDeviceCache().createdDeviceCount();
  });

  REQUIRE(runs.size() == std::size_t(kSelections));
  if(runs.front().result.skipped)
  {
    SUCCEED("Vulkan unavailable here: " + runs.front().result.skip_reason);
    return;
  }

  for(int i = 0; i < kSelections; ++i)
  {
    // Printed, not just INFO'd: the per-selection cost is the number this
    // change exists to move, and it is worth seeing on a passing run.
    std::fprintf(
        stderr, "PREVIEW-SELECTION %d: %.1f ms\n", i, runs[i].ms);
    INFO(
        "selection " << i << " took " << runs[i].ms
                     << " ms, error=" << runs[i].result.error);
    REQUIRE(runs[i].result.error.empty());
    REQUIRE(runs[i].result.outputs.size() == 1u);
    // A preview that is fast because it draws nothing is a regression.
    REQUIRE(nonDegenerate(runs[i].result.outputs[0]));
  }

  // The whole point. Zero means the box has no video-decode queue and the
  // caller fell back to a QRhi-owned device — that is the documented fallback,
  // not a failure; anything above one means the cache is not caching.
  INFO("devices created by the cache: " << devicesCreated);
  REQUIRE(devicesCreated <= 1);
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
