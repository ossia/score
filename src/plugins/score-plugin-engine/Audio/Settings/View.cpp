// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <score/widgets/SignalUtils.hpp>

#include <QComboBox>
#include <QFormLayout>
#include <QSpinBox>
#include <QStackedWidget>
#include <QCheckBox>

#include <Audio/Settings/Model.hpp>
#include <Audio/Settings/View.hpp>
namespace Audio::Settings
{
View::View() : m_widg{new QWidget}
{
  auto lay = new QFormLayout{m_widg};

  m_sw = new QStackedWidget;
  m_Driver = new QComboBox{m_widg};
  lay->addRow(tr("Driver"), m_Driver);
  connect(
      m_Driver,
      SignalUtils::QComboBox_currentIndexChanged_int(),
      this,
      [this](int i) {
        DriverChanged(
            m_Driver->itemData(i).value<AudioFactory::ConcreteKey>());
        m_sw->setCurrentIndex(i);
      });

  SETTINGS_UI_NUM_COMBOBOX_SETUP(
      "Rate", Rate, (std::vector<int>{44100, 48000, 88200, 96000, 192000}));
  SETTINGS_UI_NUM_COMBOBOX_SETUP(
      "BufferSize",
      BufferSize,
      (std::vector<int>{32, 64, 128, 256, 512, 1024, 2048}));
  SETTINGS_UI_TOGGLE_SETUP(
        "Auto-Stereo",
        AutoStereo);

  lay->addWidget(m_sw);
}

void View::addDriver(QString txt, QVariant data, QWidget* widg)
{
  m_Driver->addItem(txt, data);
  if (widg)
  {
    m_sw->addWidget(widg);
  }
  else
  {
    m_sw->addWidget(new QWidget);
  }
}

QWidget* View::getWidget()
{
  return m_widg;
}

void View::setDriver(AudioFactory::ConcreteKey k)
{
  int idx = m_Driver->findData(QVariant::fromValue(k));
  if (idx != m_Driver->currentIndex())
    m_Driver->setCurrentIndex(idx);
}

SETTINGS_UI_NUM_COMBOBOX_IMPL(Rate)
SETTINGS_UI_NUM_COMBOBOX_IMPL(BufferSize)
SETTINGS_UI_TOGGLE_IMPL(AutoStereo)
}
