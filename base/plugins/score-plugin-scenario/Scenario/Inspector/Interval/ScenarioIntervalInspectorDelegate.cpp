// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ScenarioIntervalInspectorDelegate.hpp"

#include <Scenario/Commands/Scenario/Displacement/MoveEventMeta.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Inspector/Interval/IntervalInspectorDelegate.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <score/command/Dispatchers/OngoingCommandDispatcher.hpp>

class QWidget;

namespace Scenario
{
class IntervalInspectorWidget;
ScenarioIntervalInspectorDelegate::ScenarioIntervalInspectorDelegate(
    const IntervalModel& cst)
    : IntervalInspectorDelegate{cst}
{
}

void ScenarioIntervalInspectorDelegate::updateElements()
{
}

void ScenarioIntervalInspectorDelegate::addWidgets_pre(
    std::vector<QWidget*>& widgets, IntervalInspectorWidget* parent)
{
}

void ScenarioIntervalInspectorDelegate::addWidgets_post(
    std::vector<QWidget*>& widgets, IntervalInspectorWidget* parent)
{
}

void ScenarioIntervalInspectorDelegate::on_defaultDurationChanged(
    OngoingCommandDispatcher& dispatcher,
    const TimeVal& val,
    ExpandMode expandmode) const
{
  auto& scenario = *safe_cast<Scenario::ProcessModel*>(m_model.parent());
  dispatcher.submitCommand<Command::MoveEventMeta>(
      scenario, scenario.state(m_model.endState()).eventId(),
      m_model.date() + val, m_model.heightPercentage(), expandmode,
      LockMode::Free);
}
}
