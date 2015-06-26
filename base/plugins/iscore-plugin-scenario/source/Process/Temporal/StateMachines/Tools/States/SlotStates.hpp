#pragma once
#include "Process/Temporal/StateMachines/ScenarioStateMachineBaseStates.hpp"
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore/command/Dispatchers/SingleOngoingCommandDispatcher.hpp>

class BaseStateMachine;
class QGraphicsScene;
class ResizeSlotState : public SlotState
{
    public:
        ResizeSlotState(
                SingleOngoingCommandDispatcher& dispatcher,
                const BaseStateMachine& sm,
                QState* parent);

    private:
        SingleOngoingCommandDispatcher& m_ongoingDispatcher;
        const BaseStateMachine& m_sm;
};


class DragSlotState : public SlotState
{
    public:
        DragSlotState(
                CommandDispatcher<>& dispatcher,
                const BaseStateMachine& sm,
                const QGraphicsScene& scene,
                QState* parent);

    private:
        CommandDispatcher<> m_dispatcher;
        const BaseStateMachine& m_sm;
        const QGraphicsScene& m_scene;
};
