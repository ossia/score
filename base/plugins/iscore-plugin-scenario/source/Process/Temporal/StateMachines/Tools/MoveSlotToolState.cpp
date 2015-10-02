#include "MoveSlotToolState.hpp"

#include "Process/Temporal/StateMachines/Tools/States/ResizeSlotState.hpp"
#include "Process/Temporal/StateMachines/Tools/States/DragSlotState.hpp"
#include "Document/Constraint/Rack/RackPresenter.hpp"
#include "Document/Constraint/Rack/Slot/SlotPresenter.hpp"
#include "Document/Constraint/Rack/Slot/SlotModel.hpp"
#include "Document/Constraint/Rack/Slot/SlotOverlay.hpp"
#include "Document/Constraint/Rack/Slot/SlotView.hpp"
#include "Document/Constraint/Rack/Slot/SlotHandle.hpp"

#include "Process/Temporal/TemporalScenarioPresenter.hpp"
#include "Process/Temporal/TemporalScenarioView.hpp"
#include "Process/Temporal/StateMachines/Transitions/SlotTransitions.hpp"
#include "Process/Temporal/StateMachines/ScenarioStateMachine.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintPresenter.hpp"

#include <iscore/document/DocumentInterface.hpp>

#include <QFinalState>
#include <QGraphicsScene>
MoveSlotToolState::MoveSlotToolState(const ScenarioStateMachine& sm):
    GenericStateBase{sm}
{
    /// 1. Set the scenario in the correct state with regards to this tool.
    connect(this, &QState::entered,
            [&] ()
    {
        for(const auto& constraint : m_sm.presenter().constraints())
        {
            if(!constraint.rack()) continue;
            constraint.rack()->setDisabledSlotState();
        }
    });
    connect(this, &QState::exited,
            [&] ()
    {
        for(const auto& constraint : m_sm.presenter().constraints())
        {
            if(!constraint.rack()) continue;
            constraint.rack()->setEnabledSlotState();
        }
    });

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

    // 3. Map the external events to internal transitions of this state machine.
    auto on_press = new Press_Transition;
    this->addTransition(on_press);
    connect(on_press, &QAbstractTransition::triggered, [&] ()
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
    });

    // Forward events
    auto on_move = new Move_Transition;
    this->addTransition(on_move);
    connect(on_move, &QAbstractTransition::triggered, [&] ()
    { m_localSM.postEvent(new Move_Event); });

    auto on_release = new Release_Transition;
    this->addTransition(on_release);
    connect(on_release, &QAbstractTransition::triggered, [&] ()
    { m_localSM.postEvent(new Release_Event); });
}

void MoveSlotToolState::on_scenarioPressed()
{
}

void MoveSlotToolState::on_scenarioMoved()
{

}

void MoveSlotToolState::on_scenarioReleased()
{

}

