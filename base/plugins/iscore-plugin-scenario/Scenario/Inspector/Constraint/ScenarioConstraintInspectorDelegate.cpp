#include <Scenario/Commands/Scenario/Displacement/MoveEventMeta.hpp>

#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Inspector/Constraint/ConstraintInspectorDelegate.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include "ScenarioConstraintInspectorDelegate.hpp"
#include <iscore/command/Dispatchers/OngoingCommandDispatcher.hpp>

class QWidget;

namespace Scenario
{
class ConstraintInspectorWidget;
ScenarioConstraintInspectorDelegate::ScenarioConstraintInspectorDelegate(
        const ConstraintModel& cst):
    ConstraintInspectorDelegate{cst}
{
}

void ScenarioConstraintInspectorDelegate::updateElements()
{
}

void ScenarioConstraintInspectorDelegate::addWidgets_pre(
        std::list<QWidget*>& widgets,
        ConstraintInspectorWidget* parent)
{
}

void ScenarioConstraintInspectorDelegate::addWidgets_post(
        std::list<QWidget*>& widgets,
        ConstraintInspectorWidget* parent)
{
}

void ScenarioConstraintInspectorDelegate::on_defaultDurationChanged(
        OngoingCommandDispatcher& dispatcher,
        const TimeValue& val,
        ExpandMode expandmode) const
{
    auto& scenario = *safe_cast<Scenario::ScenarioModel*>(m_model.parent());
    dispatcher.submitCommand<Command::MoveEventMeta>(
            scenario,
            scenario.state(m_model.endState()).eventId(),
            m_model.startDate() + val,
            0.,
            expandmode);
}
}
