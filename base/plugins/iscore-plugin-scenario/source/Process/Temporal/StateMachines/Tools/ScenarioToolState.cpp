#include "ScenarioToolState.hpp"
#include "Process/Temporal/StateMachines/ScenarioStateMachine.hpp"
#include "Process/Temporal/StateMachines/ScenarioStateMachineBaseTransitions.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/Event/EventView.hpp"
#include "Document/Event/EventPresenter.hpp"

#include "Document/State/StateView.hpp"

#include "Document/TimeNode/TimeNodeModel.hpp"
#include "Document/TimeNode/TimeNodeView.hpp"
#include "Document/TimeNode/TimeNodePresenter.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/ViewModels/AbstractConstraintView.hpp"
#include "Document/Constraint/ViewModels/AbstractConstraintPresenter.hpp"
#include "Document/Constraint/ViewModels/AbstractConstraintViewModel.hpp"

#include <iostream>
ScenarioTool::ScenarioTool(const ScenarioStateMachine &sm, QState* parent) :
    ToolState{sm.scene(), parent},
    m_parentSM{sm}
{
}
id_type<EventModel> ScenarioTool::itemToEventId(const QGraphicsItem * pressedItem) const
{
    const auto& event = static_cast<const EventView*>(pressedItem)->presenter().model();
    return event.parentScenario() == &m_parentSM.model()
            ? event.id()
            : id_type<EventModel>{};
}

id_type<TimeNodeModel> ScenarioTool::itemToTimeNodeId(const QGraphicsItem *pressedItem) const
{
    const auto& timenode = static_cast<const TimeNodeView*>(pressedItem)->presenter().model();
    return timenode.parentScenario() == &m_parentSM.model()
            ? timenode.id()
            : id_type<TimeNodeModel>{};
}

id_type<ConstraintModel> ScenarioTool::itemToConstraintId(const QGraphicsItem *pressedItem) const
{
    const auto& constraint = static_cast<const AbstractConstraintView*>(pressedItem)->presenter().abstractConstraintViewModel().model();
    return constraint.parentScenario() == &m_parentSM.model()
            ? constraint.id()
            : id_type<ConstraintModel>{};
}

id_type<EventModel> ScenarioTool::itemStateToEventId(const QGraphicsItem *pressedItem) const
{
    auto state = static_cast<const StateView*>(pressedItem);
    auto cstr = static_cast<const AbstractConstraintView*>( pressedItem->parentObject());
    if (cstr->startState() == state )
        return cstr->presenter().model().startEvent();

    return cstr->presenter().model().endEvent();
}
