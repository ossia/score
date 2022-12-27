// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ExecutorView.hpp"

#include <score/widgets/FormWidget.hpp>
#include <score/widgets/SignalUtils.hpp>

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

  //auto group = new QGroupBox{tr("Advanced"), m_widg};

  //{ // general settings
  auto lay = m_widg->layout();

  SETTINGS_UI_TOGGLE_SETUP(
      "Enable listening during execution\nIf this is disabled, the device explorer UI "
      "will not update the received values when the score is executing to save "
      "resources.",
      ExecutionListening);
  SETTINGS_UI_TOGGLE_SETUP(
      "Logging\nIf this is enabled, the inputs and outputs of the selected process will "
      "be logged in the message log.",
      Logging);
  SETTINGS_UI_TOGGLE_SETUP(
      "Benchmark\nIf this is enabled, processes will show their relative resource usage "
      "at the top right.",
      Bench);
  //lay->addRow(group);
  //}
  // advanced settings
  /*
  auto lay = new QFormLayout;
  lay->setSpacing(10);
  group->setLayout(lay);
*/
  // SETTINGS_UI_COMBOBOX_SETUP("Tick policy", Tick, TickPolicies{});
  // SETTINGS_UI_COMBOBOX_SETUP(
  //     "Scheduling policy", Scheduling, SchedulingPolicies{});
  // SETTINGS_UI_COMBOBOX_SETUP("Ordering policy", Ordering, OrderingPolicies{});
  // SETTINGS_UI_COMBOBOX_SETUP("Merging policy", Merging, MergingPolicies{});
  // SETTINGS_UI_COMBOBOX_SETUP("Commit policy", Commit, CommitPolicies{});

  SETTINGS_UI_TOGGLE_SETUP(
      "Parallel\nIf this is enabled, exeuction will be separated across multiple "
      "threads.",
      Parallel);
  SETTINGS_UI_SPINBOX_SETUP("Threads", Threads);
  m_Threads->setRange(2, 128);
  if(!m_Parallel->isChecked())
    m_Threads->setEnabled(false);
  connect(m_Parallel, &QCheckBox::stateChanged, this, [=] {
    m_Threads->setEnabled(m_Parallel->isChecked());
  });

  // SETTINGS_UI_TOGGLE_SETUP("Use Score order", ScoreOrder);

  SETTINGS_UI_TOGGLE_SETUP(
      "Value compilation\nWhen doing 'Play from here', score will try to put all "
      "the "
      "devices in the state they should be if execution had gone through the whole "
      "score up to this point.",
      ValueCompilation);
  SETTINGS_UI_TOGGLE_SETUP(
      "Transport value compilation\nSame as above, but also when doing transport if we "
      "are already playing.",
      TransportValueCompilation);
}

SETTINGS_UI_COMBOBOX_IMPL(Tick)
SETTINGS_UI_COMBOBOX_IMPL(Scheduling)
SETTINGS_UI_COMBOBOX_IMPL(Ordering)
SETTINGS_UI_COMBOBOX_IMPL(Merging)
SETTINGS_UI_COMBOBOX_IMPL(Commit)

SETTINGS_UI_SPINBOX_IMPL(Threads)

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
