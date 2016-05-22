#include <Scenario/Document/Constraint/Rack/RackPresenter.hpp>
#include <Scenario/Palette/Tools/States/DragSlotState.hpp>
#include <Scenario/Palette/Tools/States/ResizeSlotState.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioPresenter.hpp>
#include <iscore/tools/std/Optional.hpp>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QState>
#include <QTransform>
#include <algorithm>

#include "MoveSlotToolState.hpp"
#include <Scenario/Document/Constraint/Rack/Slot/SlotHandle.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotModel.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotOverlay.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotPresenter.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotView.hpp>
#include <Scenario/Palette/ScenarioPalette.hpp>
#include <Scenario/Palette/ScenarioPaletteBaseEvents.hpp>
#include <Scenario/Palette/Transitions/SlotTransitions.hpp>
#include <iscore/statemachine/StateMachineUtils.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

namespace Scenario
{
MoveSlotTool::MoveSlotTool(const ToolPalette& sm):
    m_sm{sm}
{
    /// 1. Set the scenario in the correct state with regards to this tool.
    /// (done in activate() / desactivate() )

    /// 2. Setup the sub-state machine.
    m_waitState = new QState{&m_localSM};
    m_localSM.setInitialState(m_waitState);
    // Two states : one for moving the content of the slot, one for resizing with the handle.
    {
        auto dragSlot = new DragSlotState<ToolPalette>{m_sm.context().context.commandStack, m_sm, m_sm.scene(), &m_localSM};
        // Enter the state
        iscore::make_transition<ClickOnSlotOverlay_Transition>(
                    m_waitState,
                    dragSlot,
                    *dragSlot);

        dragSlot->addTransition(dragSlot, finishedState(), m_waitState);
    }

    {
        auto resizeSlot = new ResizeSlotState<ToolPalette>{m_sm.context().context.commandStack, m_sm, &m_localSM};
        iscore::make_transition<ClickOnSlotHandle_Transition>(
                    m_waitState,
                    resizeSlot,
                    *resizeSlot);

        resizeSlot->addTransition(resizeSlot, finishedState(), m_waitState);
    }

    m_localSM.start();
}

void MoveSlotTool::on_pressed(QPointF scenePoint)
{
    auto item = m_sm.scene().itemAt(scenePoint, QTransform());
    if(!item)
        return;

    switch(item->type())
    {
        case SlotOverlay::static_type():
            m_localSM.postEvent(new ClickOnSlotOverlay_Event{
                                    static_cast<SlotOverlay*>(item)->slotView().presenter.model()});
            break;
        case SlotHandle::static_type():
            m_localSM.postEvent(new ClickOnSlotHandle_Event{
                                    static_cast<SlotHandle*>(item)->slotView().presenter.model()});
            break;
    }
}

void MoveSlotTool::on_moved()
{
    m_localSM.postEvent(new iscore::Move_Event);
}

void MoveSlotTool::on_released()
{
    m_localSM.postEvent(new iscore::Release_Event);
}

void MoveSlotTool::on_cancel()
{
    m_localSM.postEvent(new iscore::Cancel_Event);
    QApplication::processEvents();
}

void MoveSlotTool::activate()
{
    for(const auto& constraint : m_sm.presenter().getConstraints())
    {
        if(!constraint.rack()) continue;
        constraint.rack()->setDisabledSlotState();
    }
}

void MoveSlotTool::desactivate()
{
    for(const auto& constraint : m_sm.presenter().getConstraints())
    {
        if(!constraint.rack()) continue;
        constraint.rack()->setEnabledSlotState();
    }
}
}
