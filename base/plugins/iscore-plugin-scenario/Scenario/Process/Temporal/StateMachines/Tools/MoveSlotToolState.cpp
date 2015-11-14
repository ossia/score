#include "MoveSlotToolState.hpp"

#include <Scenario/Process/Temporal/StateMachines/Tools/States/ResizeSlotState.hpp>
#include <Scenario/Process/Temporal/StateMachines/Tools/States/DragSlotState.hpp>
#include <Scenario/Document/Constraint/Rack/RackPresenter.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotPresenter.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotModel.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotOverlay.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotView.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotHandle.hpp>

#include <Scenario/Process/Temporal/TemporalScenarioPresenter.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioView.hpp>
#include <Scenario/Process/Temporal/StateMachines/Transitions/SlotTransitions.hpp>
#include <Scenario/Process/Temporal/StateMachines/ScenarioStateMachine.hpp>
#include <Scenario/Document/Constraint/ViewModels/Temporal/TemporalConstraintPresenter.hpp>

#include <iscore/document/DocumentInterface.hpp>

#include <QFinalState>
#include <QGraphicsScene>
MoveSlotToolState::MoveSlotToolState(const ScenarioStateMachine& sm):
    m_sm{sm}
{
    /// 1. Set the scenario in the correct state with regards to this tool.
    /// (done in start () / stop () )

    /// 2. Setup the sub-state machine.
    m_waitState = new QState{&m_localSM};
    m_localSM.setInitialState(m_waitState);
    // Two states : one for moving the content of the slot, one for resizing with the handle.
    {
        auto dragSlot = new DragSlotState{m_sm.commandStack(), m_sm, m_sm.scene(), &m_localSM};
        // Enter the state
        make_transition<ClickOnSlotOverlay_Transition>(
                    m_waitState,
                    dragSlot,
                    *dragSlot);

        dragSlot->addTransition(dragSlot, SIGNAL(finished()), m_waitState);
    }

    {
        auto resizeSlot = new ResizeSlotState{m_sm.commandStack(), m_sm, &m_localSM};
        make_transition<ClickOnSlotHandle_Transition>(
                    m_waitState,
                    resizeSlot,
                    *resizeSlot);

        resizeSlot->addTransition(resizeSlot, SIGNAL(finished()), m_waitState);
    }
}

void MoveSlotToolState::on_pressed()
{
    auto item = m_sm.scene().itemAt(m_sm.scenePoint, QTransform());
    if(auto overlay = dynamic_cast<SlotOverlay*>(item))
    {
        m_localSM.postEvent(new ClickOnSlotOverlay_Event{
                                overlay->slotView().presenter.model()});
    }
    else if(auto handle = dynamic_cast<SlotHandle*>(item))
    {
        m_localSM.postEvent(new ClickOnSlotHandle_Event{
                                handle->slotView().presenter.model()});
    }
}

void MoveSlotToolState::on_moved()
{
    m_localSM.postEvent(new Move_Event);
}

void MoveSlotToolState::on_released()
{
     m_localSM.postEvent(new Release_Event);
}

void MoveSlotToolState::start()
{
    if(!m_localSM.isRunning())
        m_localSM.start();

    for(const auto& constraint : m_sm.presenter().constraints())
    {
        if(!constraint.rack()) continue;
        constraint.rack()->setDisabledSlotState();
    }
}

void MoveSlotToolState::stop()
{
    if(m_localSM.isRunning())
        m_localSM.stop();

    for(const auto& constraint : m_sm.presenter().constraints())
    {
        if(!constraint.rack()) continue;
        constraint.rack()->setEnabledSlotState();
    }
}

