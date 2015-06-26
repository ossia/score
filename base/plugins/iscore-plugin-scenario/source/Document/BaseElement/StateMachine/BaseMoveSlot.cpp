#include "BaseMoveSlot.hpp"
#include "Process/Temporal/StateMachines/Tools/States/SlotStates.hpp"
#include "Commands/Constraint/Box/MoveSlot.hpp"
#include "Commands/Constraint/Box/Slot/ResizeSlotVertically.hpp"
#include "Document/Constraint/Box/Slot/SlotView.hpp"
#include "Document/Constraint/Box/Slot/SlotOverlay.hpp"
#include "Document/Constraint/Box/Slot/SlotHandle.hpp"
#include "Document/Constraint/Box/BoxPresenter.hpp"
#include "Document/Constraint/Box/Slot/SlotPresenter.hpp"
#include "Document/Constraint/Box/Slot/SlotModel.hpp"

#include "Commands/Constraint/Box/SwapSlots.hpp"
#include "Process/Temporal/StateMachines/Transitions/SlotTransitions.hpp"

#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>

#include <QFinalState>
#include <QGraphicsScene>
#include "Process/Temporal/StateMachines/ScenarioStateMachine.hpp"

BaseMoveSlot::BaseMoveSlot(
        const QGraphicsScene& scene,
        iscore::CommandStack& stack,
        BaseStateMachine& sm):
    QState{&sm},
    m_sm{sm},
    m_dispatcher{stack},
    m_ongoingDispatcher{stack},
    m_scene{scene}
{
    /// 2. Setup the sub-state machine.
    m_waitState = new QState{&m_localSM};
    m_localSM.setInitialState(m_waitState);
    // Two states : one for moving the content of the slot, one for resizing with the handle.
    {
        auto dragSlot = new DragSlotState{m_dispatcher, m_sm, m_scene, &m_localSM};
        // Enter the state
        make_transition<ClickOnSlotOverlay_Transition>(
                    m_waitState,
                    dragSlot,
                    *dragSlot);

        dragSlot->addTransition(dragSlot, SIGNAL(finished()), m_waitState);
    }

    {
        auto resizeSlot = new ResizeSlotState{m_ongoingDispatcher, m_sm, &m_localSM};
        make_transition<ClickOnSlotHandle_Transition>(
                    m_waitState,
                    resizeSlot,
                    *resizeSlot);

        resizeSlot->addTransition(resizeSlot, SIGNAL(finished()), m_waitState);
    }

    // 3. Map the external events to internal transitions of this state machine.
    auto on_press = new Press_Transition;
    this->addTransition(on_press);
    connect(on_press, &QAbstractTransition::triggered, this, [&] ()
    {
        auto item = m_scene.itemAt(m_sm.scenePoint, QTransform());
        if(auto overlay = dynamic_cast<SlotOverlay*>(item))
        {
            m_localSM.postEvent(new ClickOnSlotOverlay_Event{
                                    iscore::IDocument::path(overlay->slotView().presenter.model())});
        }
        else if(auto handle = dynamic_cast<SlotHandle*>(item))
        {
            m_localSM.postEvent(new ClickOnSlotHandle_Event{
                                    iscore::IDocument::path(handle->slotView().presenter.model())});
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

    m_localSM.start();
}
