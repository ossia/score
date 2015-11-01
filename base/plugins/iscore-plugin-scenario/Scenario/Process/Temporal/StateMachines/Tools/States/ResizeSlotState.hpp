#pragma once
#include <Scenario/Process/Temporal/StateMachines/ScenarioStateMachineBaseStates.hpp>
#include <iscore/command/Dispatchers/SingleOngoingCommandDispatcher.hpp>

#include <Scenario/Commands/Constraint/Rack/Slot/ResizeSlotVertically.hpp>
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
