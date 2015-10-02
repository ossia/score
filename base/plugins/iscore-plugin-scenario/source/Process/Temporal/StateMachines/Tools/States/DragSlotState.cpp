#include "DragSlotState.hpp"

#include "Process/Temporal/StateMachines/ScenarioStateMachine.hpp"
#include "Process/Temporal/StateMachines/ScenarioStateMachineBaseTransitions.hpp"

#include "Document/Constraint/Rack/RackModel.hpp"

#include "Document/Constraint/Rack/Slot/SlotModel.hpp"
#include "Document/Constraint/Rack/Slot/SlotOverlay.hpp"
#include "Document/Constraint/Rack/Slot/SlotView.hpp"
#include "Document/Constraint/Rack/Slot/SlotPresenter.hpp"

#include "Commands/Constraint/Rack/MoveSlot.hpp"
#include "Commands/Constraint/Rack/SwapSlots.hpp"

#include <iscore/document/DocumentInterface.hpp>

#include <QFinalState>
#include <QGraphicsScene>
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
            auto& baseSlot = this->currentSlot.find();
            auto& releasedSlot = overlay->slotView().presenter.model();
            // If it is the same, we do nothing.
            // If it is another (in the same rack), we swap them
            if(releasedSlot.id() != baseSlot.id()
                    && releasedSlot.parent() == baseSlot.parent())
            {
                auto cmd = new Scenario::Command::SwapSlots{
                        *safe_cast<RackModel*>(releasedSlot.parent()), // Rack
                        baseSlot.id(), releasedSlot.id()};
                m_dispatcher.submitCommand(cmd);
            }
        }
        else
        {
            // We throw it
            auto cmd = new Scenario::Command::RemoveSlotFromRack(Path<SlotModel>{this->currentSlot});
            m_dispatcher.submitCommand(cmd);
            return;
        }
    });
}
