// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <Gfx/Settings/Model.hpp>
#include <Gfx/Settings/Presenter.hpp>
#include <Gfx/Settings/View.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/widgets/FormWidget.hpp>
#include <score/widgets/SignalUtils.hpp>

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QSignalBlocker>
#include <QSpinBox>

#include <functional>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Gfx::Settings::View)
namespace Gfx::Settings
{
namespace
{
struct GpuComboBox final : QComboBox
{
  using QComboBox::QComboBox;
  std::function<void()> populate;
  void showPopup() override
  {
    if(populate)
      populate();
    QComboBox::showPopup();
  }
};
}

View::View()
{
  m_widg = new score::FormWidget{tr("Graphics")};

  auto lay = m_widg->layout();
  SETTINGS_UI_COMBOBOX_SETUP("Graphics API", GraphicsApi, GraphicsApis{});
  SETTINGS_UI_COMBOBOX_SETUP(
      "Hardware Video Decoding", HardwareDecode, HardwareVideoDecoder{});

  {
    // "Auto" (0) lets the decoder pick a pool per codec; a number only sizes
    // it, the threading model stays the decoder's choice.
    m_DecodingThreads = new QComboBox{m_widg};
    m_DecodingThreads->addItem(tr("Auto"), 0);
    static constexpr int t_values[]{1, 2, 4, 6, 8, 12, 16, 24, 32, 48, 64};
    for(int v : t_values)
      m_DecodingThreads->addItem(QString::number(v), v);
    m_DecodingThreads->setToolTip(
        tr("Threads per software video decoder. Auto picks frame or slice threads "
           "and their count per codec."));
    lay->addRow(tr("Decoding threads"), m_DecodingThreads);
    connect(
        m_DecodingThreads, SignalUtils::QComboBox_currentIndexChanged_int(), this,
        [this](int i) { DecodingThreadsChanged(m_DecodingThreads->itemData(i).toInt()); });
  }

  static constexpr int aa_values[]{1, 2, 4, 8, 16};
  SETTINGS_UI_NUM_COMBOBOX_SETUP("Multisampling AA", Samples, aa_values);
  SETTINGS_UI_DOUBLE_SPINBOX_SETUP("Rate (if no VSync)", Rate);
  m_Rate->setRange(1., 1000.);
  SETTINGS_UI_TOGGLE_SETUP("VSync", VSync);

  static constexpr int buffers_values[]{1, 2, 3};
  SETTINGS_UI_NUM_COMBOBOX_SETUP("Buffer count", Buffers, buffers_values);

  {
    auto gpuCombo = new GpuComboBox{m_widg};
    gpuCombo->populate = [this] { populateGpus(); };
    m_GpuIndex = gpuCombo;
    m_GpuIndex->addItem(tr("Default"), -1);
    m_GpuIndex->setToolTip(
        tr("GPU used by the outputs created from now on. On Vulkan, the physical "
           "device; on Direct3D 11 and 12, the adapter of that same GPU. No effect "
           "on OpenGL and Metal. QT_VK_PHYSICAL_DEVICE_INDEX or QT_D3D_ADAPTER_INDEX "
           "set at startup win over this setting."));
    lay->addRow(tr("GPU"), m_GpuIndex);
    connect(
        m_GpuIndex, SignalUtils::QComboBox_currentIndexChanged_int(), this,
        [this](int i) { GpuIndexChanged(m_GpuIndex->itemData(i).toInt()); });
  }
}

static QString gpuLabel(const GpuDescription& g)
{
  QString features = g.type;
  if(g.presentBarrier)
    features += features.isEmpty() ? QStringLiteral("present barrier")
                                   : QStringLiteral(", present barrier");
  const QString label = QString::number(g.index) + QStringLiteral(": ") + g.name;
  return features.isEmpty() ? label : label + QStringLiteral(" (") + features + ')';
}

void View::populateGpus()
{
  if(m_gpusPopulated)
    return;
  m_gpusPopulated = true;

  const QSignalBlocker block{m_GpuIndex};
  const int current = m_GpuIndex->currentData().toInt();
  while(m_GpuIndex->count() > 1)
    m_GpuIndex->removeItem(1);

  const auto gpus = availableVulkanGpus();
  bool currentFound = current < 0;
  for(const auto& g : gpus)
  {
    m_GpuIndex->addItem(gpuLabel(g), g.index);
    if(g.index == current)
      currentFound = true;
  }
  if(!currentFound)
    m_GpuIndex->addItem(tr("%1 (unavailable)").arg(current), current);
  m_GpuIndex->setCurrentIndex(std::max(0, m_GpuIndex->findData(current)));
}

void View::setGpuIndex(int val)
{
  if(val < 0)
    val = -1;
  if(val >= 0)
    populateGpus();
  int idx = m_GpuIndex->findData(val);
  if(idx == -1)
  {
    m_GpuIndex->addItem(tr("%1 (unavailable)").arg(val), val);
    idx = m_GpuIndex->count() - 1;
  }
  if(idx != m_GpuIndex->currentIndex())
    m_GpuIndex->setCurrentIndex(idx);
}

QWidget* View::getWidget()
{
  return m_widg;
}

SETTINGS_UI_COMBOBOX_IMPL(GraphicsApi)
SETTINGS_UI_COMBOBOX_IMPL(HardwareDecode)
SETTINGS_UI_NUM_COMBOBOX_IMPL(DecodingThreads)
SETTINGS_UI_NUM_COMBOBOX_IMPL(Samples)
SETTINGS_UI_DOUBLE_SPINBOX_IMPL(Rate)
SETTINGS_UI_TOGGLE_IMPL(VSync)
SETTINGS_UI_NUM_COMBOBOX_IMPL(Buffers)
}
