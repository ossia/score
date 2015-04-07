#pragma once
#include "Process/Temporal/StateMachines/Tools/GenericToolState.hpp"

class MoveDeckToolState : public GenericToolState
{
    public:
        MoveDeckToolState(ScenarioStateMachine& sm);

        void on_scenarioPressed() override;
        void on_scenarioMoved() override;
        void on_scenarioReleased() override;

    private:
        QState* m_waitState{};
};
