// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ExecutorView.hpp"
#include <QFormLayout>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <iscore/widgets/SignalUtils.hpp>

namespace Engine
{
namespace Execution
{
namespace Settings
{

View::View() : m_widg{new QWidget}
{
  auto lay = new QFormLayout;
  m_widg->setLayout(lay);

  m_sb = new QSpinBox;
  m_sb->setMinimum(1);
  m_sb->setMaximum(1000);
  lay->addRow(tr("Granularity"), m_sb);

  m_cb = new QComboBox;
  lay->addRow(tr("Clock source"), m_cb);

  m_listening = new QCheckBox;
  lay->addRow(tr("Enable listening during execution"), m_listening);

  connect(
      m_sb, SignalUtils::QSpinBox_valueChanged_int(), this,
      &View::rateChanged);

  connect(
      m_cb, SignalUtils::QComboBox_currentIndexChanged_int(), this,
      [this](int i) {
        emit clockChanged(
            m_cb->itemData(i)
                .value<ClockManagerFactory::ConcreteKey>());
      });

  connect(
      m_listening, &QCheckBox::toggled, this,
      &View::executionListeningChanged);
}

void View::setRate(int val)
{
  if (val != m_sb->value())
    m_sb->setValue(val);
}
void View::setExecutionListening(bool val)
{
  if (val != m_listening->isChecked())
    m_listening->setChecked(val);
}

void View::setClock(ClockManagerFactory::ConcreteKey k)
{
  int idx = m_cb->findData(QVariant::fromValue(k));
  if (idx != m_cb->currentIndex())
    m_cb->setCurrentIndex(idx);
}

void View::populateClocks(
    const std::map<QString, ClockManagerFactory::ConcreteKey>& map)
{
  for (auto& elt : map)
  {
    m_cb->addItem(elt.first, QVariant::fromValue(elt.second));
  }
}

QWidget* View::getWidget()
{
  return m_widg;
}
}
}
}
