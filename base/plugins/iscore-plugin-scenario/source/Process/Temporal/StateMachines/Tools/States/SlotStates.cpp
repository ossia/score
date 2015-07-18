#include "SlotStates.hpp"

#include "Process/Temporal/StateMachines/ScenarioStateMachine.hpp"
#include "Process/Temporal/StateMachines/ScenarioStateMachineBaseTransitions.hpp"

#include "Document/Constraint/Rack/Slot/SlotModel.hpp"
#include "Document/Constraint/Rack/Slot/SlotOverlay.hpp"
#include "Document/Constraint/Rack/Slot/SlotView.hpp"
#include "Document/Constraint/Rack/Slot/SlotPresenter.hpp"

#include "Commands/Constraint/Rack/MoveSlot.hpp"
#include "Commands/Constraint/Rack/SwapSlots.hpp"

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
        m_originalHeight = this->currentSlot.find<SlotModel>().height();
    });

    connect(move, &QAbstractState::entered, [=] ( )
    {
        auto val = std::max(20.0,
                            m_originalHeight + (m_sm.scenePoint.y() - m_originalPoint.y()));

        m_ongoingDispatcher.submitCommand(
                    ObjectPath{this->currentSlot},
                    val);
        return;
    });

    connect(release, &QAbstractState::entered, [=] ()
    {
        m_ongoingDispatcher.commit();
    });
}


DragSlotState::DragSlotState(iscore::CommandStack& stack,
        const BaseStateMachine &sm,
        const QGraphicsScene &scene,
        QState *parent):
    SlotState{parent},
    m_dispatcher{stack},
    m_sm{sm},
    m_scene{scene}
{
    // States :
    auto press = new QState{this};
    this->setInitialState(press);
    auto move = new QState{this};
    auto release = new QFinalState{this};

    make_transition<Move_Transition>(press, move);
    make_transition<Release_Transition>(press, release);
    make_transition<Release_Transition>(move, release);

    connect(release, &QAbstractState::entered, [=] ( )
    {
        auto overlay = dynamic_cast<SlotOverlay*>(m_scene.itemAt(m_sm.scenePoint, QTransform()));
        if(overlay)
        {
            auto& baseSlot = this->currentSlot.find<SlotModel>();
            auto& releasedSlot = overlay->slotView().presenter.model();
            // If it is the same, we do nothing.
            // If it is another (in the same rack), we swap them
            if(releasedSlot.id() != baseSlot.id()
                    && releasedSlot.parent() == baseSlot.parent())
            {
                auto cmd = new Scenario::Command::SwapSlots{
                        iscore::IDocument::path(releasedSlot.parent()), // Rack
                        baseSlot.id(), releasedSlot.id()};
                m_dispatcher.submitCommand(cmd);
            }
        }
        else
        {
            // We throw it
            auto cmd = new Scenario::Command::RemoveSlotFromRack(ObjectPath{this->currentSlot});
            m_dispatcher.submitCommand(cmd);
            return;
        }
    });
}
