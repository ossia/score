#pragma once
#include "Process/Temporal/StateMachines/ScenarioStateMachineBaseStates.hpp"
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore/command/Dispatchers/SingleOngoingCommandDispatcher.hpp>

#include "Commands/Constraint/Rack/Slot/ResizeSlotVertically.hpp"
class BaseStateMachine;
class QGraphicsScene;
class ResizeSlotState : public SlotState
{
    public:
        ResizeSlotState(
                iscore::CommandStack& stack,
                const BaseStateMachine& sm,
                QState* parent);

    private:
        SingleOngoingCommandDispatcher<Scenario::Command::ResizeSlotVertically> m_ongoingDispatcher;
        const BaseStateMachine& m_sm;
};

// TODO split file
class DragSlotState : public SlotState
{
    public:
        DragSlotState(
                iscore::CommandStack& stack,
                const BaseStateMachine& sm,
                const QGraphicsScene& scene,
                QState* parent);

    private:
        CommandDispatcher<> m_dispatcher;
        const BaseStateMachine& m_sm;
        const QGraphicsScene& m_scene;
};
