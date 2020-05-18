// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ExecutorView.hpp"

#include <score/widgets/SignalUtils.hpp>
#include <score/widgets/FormWidget.hpp>

#include <QCheckBox>
#include <QFormLayout>
#include <QGroupBox>

namespace Execution
{
namespace Settings
{

View::View()
{
  m_widg = new score::FormWidget{tr("Execution")};

  auto group = new QGroupBox{tr("Advanced"), m_widg};

  { // general settings
    auto lay = m_widg->layout();

    SETTINGS_UI_TOGGLE_SETUP("Enable listening during execution", ExecutionListening);
    SETTINGS_UI_TOGGLE_SETUP("Logging", Logging);
    SETTINGS_UI_TOGGLE_SETUP("Benchmark", Bench);
    lay->addRow(group);
  }
  // advanced settings

  auto lay = new QFormLayout;
  lay->setSpacing(10);
  group->setLayout(lay);

  SETTINGS_UI_COMBOBOX_SETUP("Tick policy", Tick, TickPolicies{});
  SETTINGS_UI_COMBOBOX_SETUP("Scheduling policy", Scheduling, SchedulingPolicies{});
  SETTINGS_UI_COMBOBOX_SETUP("Ordering policy", Ordering, OrderingPolicies{});
  SETTINGS_UI_COMBOBOX_SETUP("Merging policy", Merging, MergingPolicies{});
  SETTINGS_UI_COMBOBOX_SETUP("Commit policy", Commit, CommitPolicies{});

  SETTINGS_UI_TOGGLE_SETUP("Parallel", Parallel);
  SETTINGS_UI_TOGGLE_SETUP("Use Score order", ScoreOrder);

  SETTINGS_UI_TOGGLE_SETUP("Value compilation", ValueCompilation);
  SETTINGS_UI_TOGGLE_SETUP("Transport value compilation", TransportValueCompilation);
}

SETTINGS_UI_COMBOBOX_IMPL(Tick)
SETTINGS_UI_COMBOBOX_IMPL(Scheduling)
SETTINGS_UI_COMBOBOX_IMPL(Ordering)
SETTINGS_UI_COMBOBOX_IMPL(Merging)
SETTINGS_UI_COMBOBOX_IMPL(Commit)

SETTINGS_UI_TOGGLE_IMPL(ExecutionListening)
SETTINGS_UI_TOGGLE_IMPL(ScoreOrder)
SETTINGS_UI_TOGGLE_IMPL(Parallel)
SETTINGS_UI_TOGGLE_IMPL(Logging)
SETTINGS_UI_TOGGLE_IMPL(Bench)
SETTINGS_UI_TOGGLE_IMPL(ValueCompilation)
SETTINGS_UI_TOGGLE_IMPL(TransportValueCompilation)

QWidget* View::getWidget()
{
  return m_widg;
}
}
}
