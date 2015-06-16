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
const id_type<EventModel> &ScenarioTool::itemToEventId(const QGraphicsItem * pressedItem) const
{
    return static_cast<const EventView*>(pressedItem)->presenter().model().id();
}

const id_type<TimeNodeModel> &ScenarioTool::itemToTimeNodeId(const QGraphicsItem *pressedItem) const
{
    return static_cast<const TimeNodeView*>(pressedItem)->presenter().model().id();
}

const id_type<ConstraintModel> &ScenarioTool::itemToConstraintId(const QGraphicsItem *pressedItem) const
{
    return static_cast<const AbstractConstraintView*>(pressedItem)->presenter().abstractConstraintViewModel().model().id();
}

const id_type<EventModel> &ScenarioTool::itemStateToEventId(const QGraphicsItem *pressedItem) const
{
    auto state = static_cast<const StateView*>(pressedItem);
    auto cstr = static_cast<const AbstractConstraintView*>( pressedItem->parentObject());
    if (cstr->startState() == state )
        return cstr->presenter().model().startEvent();

    return cstr->presenter().model().endEvent();
}
