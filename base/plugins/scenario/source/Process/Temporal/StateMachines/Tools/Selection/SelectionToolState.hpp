#pragma once
#include "Process/Temporal/StateMachines/Tools/GenericToolState.hpp"

class SelectionToolState : public GenericToolState
{
    public:
        SelectionToolState(ScenarioStateMachine& sm);

        void on_scenarioPressed() override;
        void on_scenarioMoved() override;
        void on_scenarioReleased() override;

    private:
        QState* m_singleSelection{};
        QState* m_multiSelection{};

};
