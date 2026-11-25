#include <score_test/Gfx.hpp>

#include <score/gfx/Vulkan.hpp>

#include <Gfx/Graph/GpuSelection.hpp>
#include <Gfx/Graph/RenderState.hpp>
#include <Gfx/Settings/Model.hpp>
#include <Gfx/Settings/View.hpp>

#include <rhi/qrhi.h>

#include <QComboBox>
#if QT_HAS_VULKAN
#include <QVulkanFunctions>
#include <QVulkanInstance>
#endif

#include <catch2/catch_test_macros.hpp>

#include <set>

using namespace score::test::gfx;

#if QT_HAS_VULKAN
namespace
{
struct ResetGpu
{
  ~ResetGpu() { score::gfx::selectGpu({}); }
};

std::vector<VkPhysicalDevice> physicalDevices()
{
  std::vector<VkPhysicalDevice> devs;
  auto* inst = score::gfx::staticVulkanInstance();
  if(!inst || !inst->isValid())
    return devs;
  uint32_t count = 0;
  inst->functions()->vkEnumeratePhysicalDevices(inst->vkInstance(), &count, nullptr);
  devs.resize(count);
  inst->functions()->vkEnumeratePhysicalDevices(inst->vkInstance(), &count, devs.data());
  return devs;
}

VkPhysicalDevice renderedOn()
{
  auto st = score::gfx::createRenderState(score::gfx::Vulkan, QSize{16, 16}, nullptr);
  if(!st || !st->rhi || st->rhi->backend() != QRhi::Vulkan)
    return VK_NULL_HANDLE;
  return static_cast<const QRhiVulkanNativeHandles*>(st->rhi->nativeHandles())->physDev;
}

struct Picks
{
  bool skipped{};
  std::vector<std::pair<VkPhysicalDevice, VkPhysicalDevice>> chosen;
  VkPhysicalDevice byDefault{}, unknown{};
  bool unknownRefused{};
};

Picks pickEach()
{
  Picks p;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    std::string probed;
    if(!probe_api(score::gfx::Vulkan, probed))
    {
      p.skipped = true;
      return;
    }
    ResetGpu reset;
    const auto devs = physicalDevices();
    for(const auto& g : score::gfx::availableGpus())
    {
      score::gfx::selectGpu(g.key);
      p.chosen.emplace_back(devs[std::size_t(g.index)], renderedOn());
    }
    score::gfx::selectGpu({});
    p.byDefault = renderedOn();
    p.unknownRefused = !score::gfx::selectGpu(QStringLiteral("no-such-gpu"));
    p.unknown = renderedOn();
  });
  return p;
}

void checkPicks(const Picks& p)
{
  if(p.skipped)
    SKIP("no Vulkan");
  REQUIRE(!p.chosen.empty());
  if(p.chosen.size() < 2)
    WARN("a single Vulkan device: selection between devices is not exercised");
  for(const auto& [want, got] : p.chosen)
    CHECK(want == got);
  CHECK(p.byDefault != VK_NULL_HANDLE);
  CHECK(p.unknownRefused);
  CHECK(p.unknown == p.byDefault);
}
}

#endif

TEST_CASE("each GPU's key is distinct and stays the same", "[gfx][settings][gpu]")
{
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    const auto& gpus = score::gfx::availableGpus();
    std::set<QString> keys;
    for(const auto& g : gpus)
    {
      CHECK(!g.key.isEmpty());
      keys.insert(g.key);
    }
    CHECK(keys.size() == gpus.size());
    CHECK(&score::gfx::availableGpus() == &gpus);
  });
}

#if QT_HAS_VULKAN
TEST_CASE("the GPU setting picks the Vulkan physical device", "[gfx][settings][gpu]")
{
  if(qEnvironmentVariableIsSet("QT_VK_PHYSICAL_DEVICE_INDEX"))
    SKIP("QT_VK_PHYSICAL_DEVICE_INDEX is set and wins over the setting");
  checkPicks(pickEach());
}

TEST_CASE(
    "the GPU setting picks the Vulkan physical device without a shared VkDevice",
    "[gfx][settings][gpu]")
{
  if(qEnvironmentVariableIsSet("QT_VK_PHYSICAL_DEVICE_INDEX"))
    SKIP("QT_VK_PHYSICAL_DEVICE_INDEX is set and wins over the setting");
  qputenv("SCORE_GFX_NO_SHARED_VKDEVICE", "1");
  const auto p = pickEach();
  qunsetenv("SCORE_GFX_NO_SHARED_VKDEVICE");
  checkPicks(p);
}

#endif

TEST_CASE("the Graphics settings' GPU selects it for the outputs", "[gfx][settings][gpu]")
{
  int selected{-2}, reset{-2};
  int expected{-1};
  score::test::run_in_gui_app([&](const score::GUIApplicationContext& ctx) {
    const auto& gpus = score::gfx::availableGpus();
    if(gpus.empty())
      return;
    auto& model = ctx.settings<Gfx::Settings::Model>();
    const auto& g = gpus.back();
    expected = g.index;
    model.setGpu(g.key);
    selected = score::gfx::selectedGpuIndex();
    model.setGpu(QString{});
    reset = score::gfx::selectedGpuIndex();
  });
  if(expected < 0)
    SKIP("no Vulkan device");
  CHECK(selected == expected);
  CHECK(reset == -1);
}

TEST_CASE("a GPU no device answers to is shown and can be reset", "[gfx][settings][gpu]")
{
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    Gfx::Settings::View view;
    QComboBox* combo{};
    for(auto* c : static_cast<score::GlobalSettingsView&>(view)
                      .getWidget()
                      ->findChildren<QComboBox*>())
      if(c->count() > 0 && c->itemText(0) == QStringLiteral("Default")
         && c->itemData(0).typeId() == QMetaType::QString)
        combo = c;
    REQUIRE(combo);
    const auto& gpus = score::gfx::availableGpus();

    std::vector<QString> changed;
    QObject::connect(
        combo, &QComboBox::currentIndexChanged, combo,
        [&](int i) { changed.push_back(combo->itemData(i).toString()); });
    view.setGpu(QStringLiteral("no-such-gpu"));
    CHECK(combo->currentData().toString() == QStringLiteral("no-such-gpu"));
    CHECK(combo->currentText().contains(QStringLiteral("Unavailable")));
    CHECK(combo->count() == int(gpus.size()) + 2);

    combo->setCurrentIndex(combo->findData(QString{}));
    REQUIRE(!changed.empty());
    CHECK(changed.back().isEmpty());

    for(const auto& g : gpus)
      CHECK(combo->findData(g.key) >= 0);

    view.setGraphicsApi(QStringLiteral("OpenGL"));
    CHECK_FALSE(combo->isEnabled());
    view.setGraphicsApi(QStringLiteral("Vulkan"));
    CHECK(combo->isEnabled());
  });
}
