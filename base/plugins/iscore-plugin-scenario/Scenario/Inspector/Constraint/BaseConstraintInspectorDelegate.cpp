#include "BaseConstraintInspectorDelegate.hpp"
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/TimeNode/Trigger/TriggerModel.hpp>
#include <Scenario/Process/ScenarioInterface.hpp>
#include <Scenario/Inspector/TimeNode/TriggerInspectorWidget.hpp>
#include <Scenario/Inspector/Constraint/ConstraintInspectorWidget.hpp>
#include <Scenario/Document/BaseElement/BaseScenario/BaseScenario.hpp>
#include <Scenario/Commands/Scenario/Displacement/MoveEventMeta.hpp>
#include <Scenario/Commands/ResizeBaseConstraint.hpp>
#include <QLabel>

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
    auto scenario = m_model.parentScenario();
    dispatcher.submitCommand<MoveEventMeta>(
            *safe_cast<ScenarioModel*>(m_model.parent()),
            scenario->state(m_model.endState()).eventId(),
            m_model.startDate() + val,
            expandmode);
}


BaseConstraintInspectorDelegate::BaseConstraintInspectorDelegate(
        const ConstraintModel& cst):
    ConstraintInspectorDelegate{cst}
{
}

void BaseConstraintInspectorDelegate::updateElements()
{
    auto scenario = m_model.parentScenario();
    auto& tn = scenario->timeNode(m_model.endTimeNode());
    m_triggerLine->updateExpression(tn.trigger()->expression());
}

void BaseConstraintInspectorDelegate::addWidgets_pre(
        std::list<QWidget*>& widgets,
        ConstraintInspectorWidget* parent)
{
    auto scenario = m_model.parentScenario();
    auto& tn = scenario->timeNode(m_model.endTimeNode());
    m_triggerLine = new TriggerInspectorWidget{tn, parent};
    m_triggerLine->HideRmButton();
    widgets.push_back(new QLabel(QObject::tr("Trigger")));
    widgets.push_back(m_triggerLine);
}

void BaseConstraintInspectorDelegate::addWidgets_post(
        std::list<QWidget*>& widgets,
        ConstraintInspectorWidget* parent)
{
    auto scenario = m_model.parentScenario();
    auto& tn = scenario->timeNode(m_model.endTimeNode());
    auto trWidg = new TriggerInspectorWidget{tn, parent};
    trWidg->HideRmButton();
    widgets.push_back(trWidg);
}

void BaseConstraintInspectorDelegate::on_defaultDurationChanged(
        OngoingCommandDispatcher& dispatcher,
        const TimeValue& val,
        ExpandMode expandmode) const
{
    auto& scenario = *safe_cast<BaseScenario*>(m_model.parent());
    dispatcher.submitCommand<MoveBaseEvent<BaseScenario>>(
                scenario,
                scenario.endEvent().id(),
                val,
                expandmode);

}
