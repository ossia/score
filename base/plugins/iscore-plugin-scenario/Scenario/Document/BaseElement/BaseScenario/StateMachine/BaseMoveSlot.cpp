#include "BaseMoveSlot.hpp"
#include <Scenario/Document/BaseElement/BaseScenario/BaseScenarioStateMachine.hpp>
#include <Scenario/Process/Temporal/StateMachines/Tools/States/ResizeSlotState.hpp>
#include <Scenario/Commands/Constraint/Rack/MoveSlot.hpp>
#include <Scenario/Commands/Constraint/Rack/Slot/ResizeSlotVertically.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotView.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotOverlay.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotHandle.hpp>
#include <Scenario/Document/Constraint/Rack/RackPresenter.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotPresenter.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotModel.hpp>

#include <Scenario/Commands/Constraint/Rack/SwapSlots.hpp>
#include <Scenario/Process/Temporal/StateMachines/Transitions/SlotTransitions.hpp>

#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>

#include <QFinalState>
#include <QGraphicsScene>

BaseMoveSlot::BaseMoveSlot(
        const QGraphicsScene& scene,
        iscore::CommandStack& stack,
        BaseScenarioStateMachine& sm):
   // QState{&sm},
    m_sm{sm},
    m_scene{scene}
{
    //Setup the sub-state machine.
    m_waitState = new QState{&m_localSM};
    m_localSM.setInitialState(m_waitState);

    {
        auto resizeSlot = new Scenario::ResizeSlotState<BaseScenarioStateMachine>{stack, m_sm, &m_localSM};
        iscore::make_transition<Scenario::ClickOnSlotHandle_Transition>(
                    m_waitState,
                    resizeSlot,
                    *resizeSlot);

        resizeSlot->addTransition(resizeSlot, SIGNAL(finished()), m_waitState);
    }

    m_localSM.start();
}

void BaseMoveSlot::on_pressed(QPointF scenePoint)
{
    auto item = m_sm.scene().itemAt(scenePoint, QTransform());
    if(!item)
        return;

    if(item->type() == SlotHandle::static_type())
    {
        m_localSM.postEvent(new ClickOnSlotHandle_Event{
                                static_cast<SlotHandle*>(item)->slotView().presenter.model()});
    }
}

void BaseMoveSlot::on_moved()
{
    m_localSM.postEvent(new iscore::Move_Event);
}

void BaseMoveSlot::on_released()
{
    m_localSM.postEvent(new iscore::Release_Event);
}

void BaseMoveSlot::on_cancel()
{
    ISCORE_TODO;
}
