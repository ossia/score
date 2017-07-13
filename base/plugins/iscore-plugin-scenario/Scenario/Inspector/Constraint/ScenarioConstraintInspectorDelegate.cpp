// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Scenario/Commands/Scenario/Displacement/MoveEventMeta.hpp>

#include "ScenarioConstraintInspectorDelegate.hpp"
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Inspector/Constraint/ConstraintInspectorDelegate.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <iscore/command/Dispatchers/OngoingCommandDispatcher.hpp>

class QWidget;

namespace Scenario
{
class ConstraintInspectorWidget;
ScenarioConstraintInspectorDelegate::ScenarioConstraintInspectorDelegate(
    const ConstraintModel& cst)
    : ConstraintInspectorDelegate{cst}
{
}

void ScenarioConstraintInspectorDelegate::updateElements()
{
}

void ScenarioConstraintInspectorDelegate::addWidgets_pre(
    std::vector<QWidget*>& widgets, ConstraintInspectorWidget* parent)
{
}

void ScenarioConstraintInspectorDelegate::addWidgets_post(
    std::vector<QWidget*>& widgets, ConstraintInspectorWidget* parent)
{
}

void ScenarioConstraintInspectorDelegate::on_defaultDurationChanged(
    OngoingCommandDispatcher& dispatcher,
    const TimeVal& val,
    ExpandMode expandmode) const
{
  auto& scenario = *safe_cast<Scenario::ProcessModel*>(m_model.parent());
  dispatcher.submitCommand<Command::MoveEventMeta>(
      scenario,
      scenario.state(m_model.endState()).eventId(),
      m_model.startDate() + val,
      m_model.heightPercentage(),
      expandmode);
}
}
