#pragma once
#include <Scenario/Palette/ScenarioPaletteBaseStates.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>

#include <Scenario/Palette/ScenarioPalette.hpp>
#include <Scenario/Palette/ScenarioPaletteBaseTransitions.hpp>

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

class QGraphicsScene;
namespace Scenario
{
template<typename ToolPalette_T>
class DragSlotState final : public SlotState
{
    public:
        DragSlotState(
                iscore::CommandStack& stack,
                const ToolPalette_T& sm,
                const QGraphicsScene& scene,
                QState* parent):
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

            iscore::make_transition<iscore::Move_Transition>(press, move);
            iscore::make_transition<iscore::Release_Transition>(press, release);
            iscore::make_transition<iscore::Release_Transition>(move, release);

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

    private:
        CommandDispatcher<> m_dispatcher;
        const ToolPalette_T& m_sm;
        const QGraphicsScene& m_scene;
};
}
