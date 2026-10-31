// The Graphics settings' GPU index selects the Vulkan physical device every
// output created afterwards renders on: score imports that device into the
// QRhi it creates, without touching the process environment, and an index no
// device answers to falls back to the default device instead of failing.
// availableVulkanGpus() lists the devices in that order, with the
// VK_NV_present_barrier probe used for frame lock.
#include <score_test/Gfx.hpp>

#include <Gfx/Graph/RenderState.hpp>
#include <Gfx/Settings/Model.hpp>
#include <Gfx/Settings/View.hpp>

#include <rhi/qrhi.h>

#include <QComboBox>

#include <catch2/catch_test_macros.hpp>

using namespace score::test::gfx;

namespace
{
struct ResetGpuIndex
{
  ~ResetGpuIndex() { Gfx::Settings::applyGpuIndex(-1); }
};

QString deviceName(int index)
{
  Gfx::Settings::applyGpuIndex(index);
  auto st = score::gfx::createRenderState(score::gfx::Vulkan, QSize{16, 16}, nullptr);
  if(!st || !st->rhi || st->rhi->backend() != QRhi::Vulkan)
    return {};
  return QString::fromUtf8(st->rhi->driverInfo().deviceName);
}
}

TEST_CASE("the GPU index setting picks the Vulkan physical device", "[gfx][settings][gpu]")
{
  if(qEnvironmentVariableIsSet("QT_VK_PHYSICAL_DEVICE_INDEX"))
    SKIP("QT_VK_PHYSICAL_DEVICE_INDEX is set and wins over the setting");

  bool skipped{};
  bool envTouched{};
  std::vector<Gfx::Settings::GpuDescription> gpus;
  std::vector<std::pair<QString, QString>> picked;
  QString defaultDevice, staleDevice;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    std::string probed;
    if(!probe_api(score::gfx::Vulkan, probed))
    {
      skipped = true;
      return;
    }
    ResetGpuIndex reset;
    gpus = Gfx::Settings::availableVulkanGpus();
    defaultDevice = deviceName(-1);
    for(const auto& g : gpus)
    {
      picked.emplace_back(g.name, deviceName(g.index));
      envTouched |= qEnvironmentVariableIsSet("QT_VK_PHYSICAL_DEVICE_INDEX")
                    || qEnvironmentVariableIsSet("QT_D3D_ADAPTER_INDEX");
    }
    staleDevice = deviceName(int(gpus.size()) + 3);
  });
  if(skipped)
    SKIP("no Vulkan");
  REQUIRE(!gpus.empty());
  for(const auto& g : gpus)
    UNSCOPED_INFO(
        g.index << ": " << g.name.toStdString() << " " << g.type.toStdString()
                << (g.presentBarrier ? " present barrier" : ""));
  if(gpus.size() < 2)
    WARN("a single Vulkan device: selection between devices is not exercised");

  CHECK(!envTouched);
  REQUIRE(picked.size() == gpus.size());
  for(const auto& [want, got] : picked)
  {
    INFO("wanted " << want.toStdString() << " got " << got.toStdString());
    CHECK(want == got);
  }

  INFO("default " << defaultDevice.toStdString() << ", stale index "
                  << staleDevice.toStdString());
  CHECK(!defaultDevice.isEmpty());
  CHECK(staleDevice == defaultDevice);
}

TEST_CASE(
    "a GPU index no device answers to is shown and can be reset", "[gfx][settings][gpu]")
{
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    Gfx::Settings::View view;
    QComboBox* combo{};
    for(auto* c : static_cast<score::GlobalSettingsView&>(view)
                      .getWidget()
                      ->findChildren<QComboBox*>())
      if(c->count() > 0 && c->itemText(0) == QStringLiteral("Default")
         && c->itemData(0).toInt() == -1)
        combo = c;
    REQUIRE(combo);
    const auto gpus = Gfx::Settings::availableVulkanGpus();
    const int stale = int(gpus.size()) + 7;

    std::vector<int> changed;
    QObject::connect(
        combo, &QComboBox::currentIndexChanged, combo,
        [&](int i) { changed.push_back(combo->itemData(i).toInt()); });
    view.setGpuIndex(stale);
    CHECK(combo->currentData().toInt() == stale);
    CHECK(combo->currentText().contains(QStringLiteral("unavailable")));
    CHECK(combo->count() == int(gpus.size()) + 2);

    combo->setCurrentIndex(combo->findData(-1));
    REQUIRE(!changed.empty());
    CHECK(changed.back() == -1);

    view.setGpuIndex(-1);
    CHECK(combo->currentData().toInt() == -1);
    for(const auto& g : gpus)
      CHECK(combo->findData(g.index) >= 0);
  });
}
