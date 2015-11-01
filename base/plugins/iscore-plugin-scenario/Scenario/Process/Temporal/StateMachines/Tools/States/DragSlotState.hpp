#pragma once
#include <Scenario/Process/Temporal/StateMachines/ScenarioStateMachineBaseStates.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>

class BaseStateMachine;
class QGraphicsScene;

class DragSlotState final : public SlotState
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
