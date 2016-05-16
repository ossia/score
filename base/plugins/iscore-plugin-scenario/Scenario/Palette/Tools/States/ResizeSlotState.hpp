#pragma once
#include <Scenario/Palette/ScenarioPaletteBaseStates.hpp>
#include <iscore/command/Dispatchers/SingleOngoingCommandDispatcher.hpp>

#include <Scenario/Commands/Constraint/Rack/Slot/ResizeSlotVertically.hpp>

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
namespace Scenario {
template<typename ToolPalette_T>
class ResizeSlotState final : public SlotState
{
    public:
        ResizeSlotState(
                iscore::CommandStackFacade& stack,
                const ToolPalette_T& sm,
                QState* parent):
            SlotState{parent},
            m_ongoingDispatcher{stack},
            m_sm{sm}
        {
            auto press = new QState{this};
            this->setInitialState(press);
            auto move = new QState{this};
            auto release = new QFinalState{this};

            iscore::make_transition<iscore::Move_Transition>(press, move);
            iscore::make_transition<iscore::Move_Transition>(move, move);
            iscore::make_transition<iscore::Release_Transition>(press, release);
            iscore::make_transition<iscore::Release_Transition>(move, release);

            connect(press, &QAbstractState::entered, [=] ()
            {
                m_originalPoint = m_sm.scenePoint;
                m_originalHeight = this->currentSlot.find().getHeight();
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

    private:
        SingleOngoingCommandDispatcher<Scenario::Command::ResizeSlotVertically> m_ongoingDispatcher;
        const ToolPalette_T& m_sm;
};
}
