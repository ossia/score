#include "ResizeSlotState.hpp"

#include <Scenario/Process/Temporal/StateMachines/ScenarioStateMachine.hpp>
#include <Scenario/Process/Temporal/StateMachines/ScenarioStateMachineBaseTransitions.hpp>

#include <Scenario/Document/Constraint/Rack/RackModel.hpp>

#include <Scenario/Document/Constraint/Rack/Slot/SlotModel.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotOverlay.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotView.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotPresenter.hpp>

#include <Scenario/Commands/Constraint/Rack/MoveSlot.hpp>
#include <Scenario/Commands/Constraint/Rack/SwapSlots.hpp>

#include <iscore/document/DocumentInterface.hpp>

#include <QFinalState>
#include <QGraphicsScene>
ResizeSlotState::ResizeSlotState(
        iscore::CommandStack& stack,
        const BaseStateMachine &sm,
        QState *parent):
    SlotState{parent},
    m_ongoingDispatcher{stack},
    m_sm{sm}
{
    auto press = new QState{this};
    this->setInitialState(press);
    auto move = new QState{this};
    auto release = new QFinalState{this};

    make_transition<Move_Transition>(press, move);
    make_transition<Move_Transition>(move, move);
    make_transition<Release_Transition>(press, release);
    make_transition<Release_Transition>(move, release);

    connect(press, &QAbstractState::entered, [=] ()
    {
        m_originalPoint = m_sm.scenePoint;
        m_originalHeight = this->currentSlot.find().height();
    });

    connect(move, &QAbstractState::entered, [=] ( )
    {
        auto val = std::max(20.0,
                            m_originalHeight + (m_sm.scenePoint.y() - m_originalPoint.y()));

        m_ongoingDispatcher.submitCommand(
                    Path<SlotModel>{this->currentSlot},
                    val);
        return;
    });

    connect(release, &QAbstractState::entered, [=] ()
    {
        m_ongoingDispatcher.commit();
    });
}


