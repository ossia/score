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

#include <Gfx/Graph/GpuSelection.hpp>
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
    m_Gpu = gpuCombo;
    m_Gpu->addItem(tr("Default"), QString{});
    m_Gpu->setToolTip(
        tr("GPU the outputs render on, with Vulkan and Direct3D 11 and 12. The score "
           "interface stays on the default GPU. QT_VK_PHYSICAL_DEVICE_INDEX or "
           "QT_D3D_ADAPTER_INDEX set at startup win over this setting."));
#if defined(Q_OS_MACOS)
    m_Gpu->setVisible(false);
#else
    lay->addRow(tr("GPU"), m_Gpu);
#endif
    connect(
        m_Gpu, SignalUtils::QComboBox_currentIndexChanged_int(), this,
        [this](int i) { GpuChanged(m_Gpu->itemData(i).toString()); });
    auto followApi = [this] {
      const QString api = m_GraphicsApi->currentText();
      m_Gpu->setEnabled(api != GraphicsApis{}.OpenGL && api != GraphicsApis{}.Metal);
    };
    connect(m_GraphicsApi, SignalUtils::QComboBox_currentIndexChanged_int(), this, followApi);
    followApi();
  }
}

static QString gpuLabel(const score::gfx::GpuDescription& g)
{
  const QString label = QString::number(g.index) + QStringLiteral(": ") + g.name;
  return g.type.isEmpty() ? label : label + QStringLiteral(" (") + g.type + ')';
}

void View::populateGpus()
{
  if(m_gpusPopulated)
    return;
  m_gpusPopulated = true;

  const QSignalBlocker block{m_Gpu};
  const QString current = m_Gpu->currentData().toString();
  while(m_Gpu->count() > 1)
    m_Gpu->removeItem(1);

  bool currentFound = current.isEmpty();
  for(const auto& g : score::gfx::availableGpus())
  {
    m_Gpu->addItem(gpuLabel(g), g.key);
    currentFound |= g.key == current;
  }
  if(!currentFound)
    m_Gpu->addItem(tr("Unavailable GPU"), current);
  m_Gpu->setCurrentIndex(std::max(0, m_Gpu->findData(current)));
}

void View::setGpu(QString val)
{
  if(!val.isEmpty())
    populateGpus();
  int idx = m_Gpu->findData(val);
  if(idx == -1)
  {
    m_Gpu->addItem(tr("Unavailable GPU"), val);
    idx = m_Gpu->count() - 1;
  }
  if(idx != m_Gpu->currentIndex())
    m_Gpu->setCurrentIndex(idx);
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
