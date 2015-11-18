#include "ScenarioToolState.hpp"
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Process/Temporal/StateMachines/ScenarioStateMachine.hpp>
#include <Scenario/Process/Temporal/StateMachines/ScenarioStateMachineBaseTransitions.hpp>

#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Event/EventView.hpp>
#include <Scenario/Document/Event/EventPresenter.hpp>

#include <Scenario/Document/State/StateView.hpp>
#include <Scenario/Document/State/StatePresenter.hpp>
#include <Scenario/Document/State/StateModel.hpp>

#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeView.hpp>
#include <Scenario/Document/TimeNode/TimeNodePresenter.hpp>

#include <Scenario/Document/Constraint/Rack/Slot/SlotHandle.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotView.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotPresenter.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotModel.hpp>

#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/ViewModels/ConstraintView.hpp>
#include <Scenario/Document/Constraint/ViewModels/ConstraintPresenter.hpp>
#include <Scenario/Document/Constraint/ViewModels/ConstraintViewModel.hpp>

#include <iostream>
namespace Scenario
{
ToolBase::ToolBase(const ToolPalette &sm) :
    GraphicsSceneToolBase<Scenario::Point>{sm.scene()},
    m_parentSM{sm}
{
}
Id<EventModel> ToolBase::itemToEventId(const QGraphicsItem * pressedItem) const
{
    const auto& event = static_cast<const EventView*>(pressedItem)->presenter().model();
    return event.parentScenario() == &m_parentSM.model()
            ? event.id()
            : Id<EventModel>{};
}

Id<TimeNodeModel> ToolBase::itemToTimeNodeId(const QGraphicsItem *pressedItem) const
{
    const auto& timenode = static_cast<const TimeNodeView*>(pressedItem)->presenter().model();
    return timenode.parentScenario() == &m_parentSM.model()
            ? timenode.id()
            : Id<TimeNodeModel>{};
}

Id<ConstraintModel> ToolBase::itemToConstraintId(const QGraphicsItem *pressedItem) const
{
    const auto& constraint = static_cast<const ConstraintView*>(pressedItem)->presenter().abstractConstraintViewModel().model();
    return constraint.parentScenario() == &m_parentSM.model()
            ? constraint.id()
            : Id<ConstraintModel>{};
}

Id<StateModel> ToolBase::itemToStateId(const QGraphicsItem *pressedItem) const
{
    const auto& state = static_cast<const StateView*>(pressedItem)->presenter().model();

    return state.parentScenario() == &m_parentSM.model()
            ? state.id()
            : Id<StateModel>{};
}

const SlotModel* ToolBase::itemToSlotFromHandle(const QGraphicsItem *pressedItem) const
{
    const auto& slot = static_cast<const SlotHandle*>(pressedItem)->slotView().presenter.model();

    return slot.parentConstraint().parentScenario() == &m_parentSM.model()
            ? &slot
            : nullptr;
}
}
