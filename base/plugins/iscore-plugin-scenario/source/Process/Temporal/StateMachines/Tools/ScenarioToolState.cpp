#include "ScenarioToolState.hpp"
#include "Process/Temporal/StateMachines/ScenarioStateMachine.hpp"
#include "Process/Temporal/StateMachines/ScenarioStateMachineBaseTransitions.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/Event/EventView.hpp"
#include "Document/Event/EventPresenter.hpp"

#include "Document/TimeNode/TimeNodeModel.hpp"
#include "Document/TimeNode/TimeNodeView.hpp"
#include "Document/TimeNode/TimeNodePresenter.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/ViewModels/AbstractConstraintView.hpp"
#include "Document/Constraint/ViewModels/AbstractConstraintPresenter.hpp"
#include "Document/Constraint/ViewModels/AbstractConstraintViewModel.hpp"

ScenarioToolState::ScenarioToolState(const ScenarioStateMachine &sm) :
    ToolState{sm.scene()},
    m_parentSM{sm}
{
}
const id_type<EventModel> &ScenarioToolState::itemToEventId(const QGraphicsItem * pressedItem) const
{
    return static_cast<const EventView*>(pressedItem)->presenter().model().id();
}

const id_type<TimeNodeModel> &ScenarioToolState::itemToTimeNodeId(const QGraphicsItem *pressedItem) const
{
    return static_cast<const TimeNodeView*>(pressedItem)->presenter().model().id();
}

const id_type<ConstraintModel> &ScenarioToolState::itemToConstraintId(const QGraphicsItem *pressedItem) const
{
    return static_cast<const AbstractConstraintView*>(pressedItem)->presenter().abstractConstraintViewModel().model().id();
}
