// The Graphics settings' GPU index selects the Vulkan physical device every
// output created afterwards renders on (QT_VK_PHYSICAL_DEVICE_INDEX, read by
// createSharedVulkanDevice and QRhi), and availableVulkanGpus() lists them in
// that order, with the VK_NV_present_barrier probe used for frame lock.
#include <score_test/Gfx.hpp>

#include <Gfx/Graph/RenderState.hpp>
#include <Gfx/Settings/Model.hpp>

#include <rhi/qrhi.h>

#include <catch2/catch_test_macros.hpp>

using namespace score::test::gfx;

TEST_CASE("the GPU index setting picks the Vulkan physical device", "[gfx][settings][gpu]")
{
  bool skipped{};
  std::vector<Gfx::Settings::GpuDescription> gpus;
  std::vector<std::pair<QString, QString>> picked;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    std::string probed;
    if(!probe_api(score::gfx::Vulkan, probed))
    {
      skipped = true;
      return;
    }
    gpus = Gfx::Settings::availableVulkanGpus();
    for(const auto& g : gpus)
    {
      Gfx::Settings::applyGpuIndex(g.index);
      auto st = score::gfx::createRenderState(score::gfx::Vulkan, QSize{16, 16}, nullptr);
      if(st && st->rhi)
        picked.emplace_back(g.name, QString::fromUtf8(st->rhi->driverInfo().deviceName));
    }
    Gfx::Settings::applyGpuIndex(-1);
  });
  if(skipped)
    SKIP("no Vulkan");
  REQUIRE(!gpus.empty());
  for(const auto& g : gpus)
    INFO(g.index << ": " << g.name.toStdString() << " " << g.type.toStdString()
                 << (g.presentBarrier ? " present barrier" : ""));
  for(const auto& [want, got] : picked)
  {
    INFO("wanted " << want.toStdString() << " got " << got.toStdString());
    CHECK(want == got);
  }
}
