// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ExecutorView.hpp"
#include <QFormLayout>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <score/widgets/SignalUtils.hpp>

namespace Engine
{
namespace Execution
{
namespace Settings
{

#define SETTINGS_UI_COMBOBOX_SETUP(Text, Control, Values) \
  m_ ## Control = new QComboBox{m_widg}; \
  m_ ## Control->addItems(Values); \
  lay->addRow(tr(Text), m_ ## Control); \
  connect(m_ ## Control, SignalUtils::QComboBox_currentIndexChanged_int(), this, \
  [this] (int i) { Control ## Changed( m_ ## Control->itemData(i).value<QString>() ); } );

#define SETTINGS_UI_SPINBOX_SETUP(Text, Control) \
  m_ ## Control = new QSpinBox{m_widg}; \
  lay->addRow(tr(Text), m_ ## Control); \
  connect(m_ ## Control, SignalUtils::QSpinBox_valueChanged_int(), \
          this,  &View::Control ## Changed);

#define SETTINGS_UI_TOGGLE_SETUP(Text, Control) \
  m_ ## Control = new QCheckBox{m_widg}; \
  lay->addRow(tr(Text), m_ ## Control); \
  connect(m_ ## Control, &QCheckBox::toggled, \
          this, &View::Control ## Changed);


#define SETTINGS_UI_COMBOBOX_IMPL(Control)                       \
  void View::set ## Control(QString val) {                       \
    int idx = m_ ## Control->findData(QVariant::fromValue(val)); \
    if(idx != -1 && idx != m_ ## Control->currentIndex())        \
       m_ ## Control->setCurrentIndex(idx);                      \
}

#define SETTINGS_UI_SPINBOX_IMPL(Control)                        \
  void View::set ## Control(int val) {                           \
  int cur = m_ ## Control->value();                              \
  if(cur != val)                                                 \
    m_ ## Control->setValue(val);                                \
}


#define SETTINGS_UI_TOGGLE_IMPL(Control)                        \
  void View::set ## Control(bool val) {                         \
  bool cur = m_ ## Control->isChecked();                        \
  if(cur != val)                                                \
    m_ ## Control->setChecked(val);                             \
}

View::View() : m_widg{new QWidget}
{
  auto lay = new QFormLayout;
  m_widg->setLayout(lay);

  SETTINGS_UI_COMBOBOX_SETUP("Tick policy", Tick, TickPolicies{});
  SETTINGS_UI_COMBOBOX_SETUP("Scheduling policy", Scheduling, SchedulingPolicies{});
  SETTINGS_UI_COMBOBOX_SETUP("Ordering policy", Ordering, OrderingPolicies{});
  SETTINGS_UI_COMBOBOX_SETUP("Merging policy", Merging, MergingPolicies{});
  SETTINGS_UI_COMBOBOX_SETUP("Commit policy", Commit, CommitPolicies{});

  SETTINGS_UI_TOGGLE_SETUP("Enable listening during execution", ExecutionListening);
  SETTINGS_UI_TOGGLE_SETUP("Parallel", Parallel);
  SETTINGS_UI_TOGGLE_SETUP("Use Score order", ScoreOrder);
  SETTINGS_UI_TOGGLE_SETUP("Logging", Logging);


  m_Clock = new QComboBox;
  lay->addRow(tr("Clock source"), m_Clock);

  connect(
      m_Clock, SignalUtils::QComboBox_currentIndexChanged_int(), this,
      [this](int i) {
        ClockChanged(
            m_Clock->itemData(i)
                .value<ClockManagerFactory::ConcreteKey>());
      });


  SETTINGS_UI_SPINBOX_SETUP("Rate (default clock only)", Rate);
}


SETTINGS_UI_COMBOBOX_IMPL(Tick)
SETTINGS_UI_COMBOBOX_IMPL(Scheduling)
SETTINGS_UI_COMBOBOX_IMPL(Ordering)
SETTINGS_UI_COMBOBOX_IMPL(Merging)
SETTINGS_UI_COMBOBOX_IMPL(Commit)

SETTINGS_UI_SPINBOX_IMPL(Rate)

SETTINGS_UI_TOGGLE_IMPL(ExecutionListening)
SETTINGS_UI_TOGGLE_IMPL(ScoreOrder)
SETTINGS_UI_TOGGLE_IMPL(Parallel)
SETTINGS_UI_TOGGLE_IMPL(Logging)


void View::setClock(ClockManagerFactory::ConcreteKey k)
{
  int idx = m_Clock->findData(QVariant::fromValue(k));
  if (idx != m_Clock->currentIndex())
    m_Clock->setCurrentIndex(idx);
}

void View::populateClocks(
    const std::map<QString, ClockManagerFactory::ConcreteKey>& map)
{
  for (auto& elt : map)
  {
    m_Clock->addItem(elt.first, QVariant::fromValue(elt.second));
  }
}

QWidget* View::getWidget()
{
  return m_widg;
}
}
}
}
