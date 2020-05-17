// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <score/widgets/SignalUtils.hpp>

#include <QComboBox>
#include <QCheckBox>
#include <QFormLayout>
#include <QLabel>

#include <Audio/Settings/Model.hpp>
#include <Audio/Settings/View.hpp>
namespace Audio::Settings
{
View::View()
  : m_widg{new QWidget}
{
  auto lay = new QFormLayout{m_widg};
  lay->setSpacing(10);

  // General settings
  SETTINGS_UI_TOGGLE_SETUP(
        "Auto-Stereo",
        AutoStereo);

  // Driver combo-box
  m_Driver = new QComboBox{m_widg};
  lay->addRow(tr("Driver"), m_Driver);
  auto& list = score::GUIAppContext().interfaces<AudioFactoryList>();
  for (AudioFactory& drv : list)
  {
    m_Driver->addItem(drv.prettyName(), QVariant::fromValue(drv.concreteKey()));
  }

  connect(
      m_Driver,
      SignalUtils::QComboBox_currentIndexChanged_int(),
      this,
      [this](int i) {
    const auto key = m_Driver->itemData(i).value<AudioFactory::ConcreteKey>();
    DriverChanged(key);
  });


  // Driver-specific things
  m_sw = new QWidget;
  m_sw->setLayout(new QHBoxLayout);
  lay->addWidget(m_sw);
}


QWidget* View::getWidget()
{
  return m_widg;
}

void View::setDriver(AudioFactory::ConcreteKey k)
{
  int idx = m_Driver->findData(QVariant::fromValue(k));
  if (idx != m_Driver->currentIndex())
  {
    m_Driver->setCurrentIndex(idx);
  }
}

void View::setDriverWidget(QWidget* w)
{
  delete m_curDriver;
  m_curDriver = w;

  if(w)
    m_sw->layout()->addWidget(m_curDriver);
}

void View::setRate(int val)
{
  if(m_curDriver)
  {
    if(auto cb = m_curDriver->findChild<QComboBox*>("Rate"))
    {
      int idx = cb->findData(QVariant::fromValue(val));
      if (idx != -1 && idx != cb->currentIndex())
        cb->setCurrentIndex(idx);
      else
      {
        idx = cb->findText(QString::number(val));
        if (idx != -1 && idx != cb->currentIndex())
          cb->setCurrentIndex(idx);
      }
    }
    else if(auto label = m_curDriver->findChild<QLabel*>("Rate"))
    {
      label->setText(QString::number(val));
    }
  }
}

void View::setBufferSize(int val)
{
  if(m_curDriver)
  {
    if(auto cb = m_curDriver->findChild<QComboBox*>("BufferSize"))
    {
      int idx = cb->findData(QVariant::fromValue(val));
      if (idx != -1 && idx != cb->currentIndex())
        cb->setCurrentIndex(idx);
      else
      {
        idx = cb->findText(QString::number(val));
        if (idx != -1 && idx != cb->currentIndex())
          cb->setCurrentIndex(idx);
      }
    }
    else if(auto label = m_curDriver->findChild<QLabel*>("BufferSize"))
    {
      label->setText(QString::number(val));
    }
  }
}
SETTINGS_UI_TOGGLE_IMPL(AutoStereo)
}
