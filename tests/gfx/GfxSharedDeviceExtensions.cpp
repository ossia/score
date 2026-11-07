#include <score_test/Gfx.hpp>

#include <score/gfx/Vulkan.hpp>

#include <Gfx/Graph/RenderState.hpp>

#include <rhi/qrhi.h>

#include <catch2/catch_test_macros.hpp>

#include <string_view>
#include <vector>

#if QT_HAS_VULKAN
#include <QVulkanFunctions>
#include <QVulkanInstance>

using namespace score::test::gfx;

namespace
{
struct Probe
{
  bool skipped{};
  bool supported{};
  bool resolved{};
};

Probe probeDeviceFunction(const char* extension, const char* function)
{
  Probe p;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    std::string probed;
    if(!probe_api(score::gfx::Vulkan, probed))
    {
      p.skipped = true;
      return;
    }
    auto st = score::gfx::createRenderState(score::gfx::Vulkan, QSize{16, 16}, nullptr);
    if(!st || !st->rhi || st->rhi->backend() != QRhi::Vulkan)
    {
      p.skipped = true;
      return;
    }
    auto* inst = score::gfx::staticVulkanInstance();
    const auto* h = static_cast<const QRhiVulkanNativeHandles*>(st->rhi->nativeHandles());
    uint32_t count = 0;
    inst->functions()->vkEnumerateDeviceExtensionProperties(
        h->physDev, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> exts(count);
    inst->functions()->vkEnumerateDeviceExtensionProperties(
        h->physDev, nullptr, &count, exts.data());
    for(const auto& e : exts)
      p.supported |= std::string_view{e.extensionName} == extension;

    auto getDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(
        inst->getInstanceProcAddr("vkGetDeviceProcAddr"));
    p.resolved = getDeviceProcAddr && getDeviceProcAddr(h->dev, function);
  });
  return p;
}
}

TEST_CASE(
    "the Vulkan device has fragment shading rate where the GPU does",
    "[gfx][vulkan][device]")
{
  const auto p = probeDeviceFunction(
      "VK_KHR_fragment_shading_rate", "vkCmdSetFragmentShadingRateKHR");
  if(p.skipped)
    SKIP("no Vulkan");
  if(!p.supported)
    SKIP("this GPU has no VK_KHR_fragment_shading_rate");
  CHECK(p.resolved);
}
#endif
