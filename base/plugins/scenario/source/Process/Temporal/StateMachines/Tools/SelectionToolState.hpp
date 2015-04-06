#pragma once
#include "GenericToolState.hpp"

class SelectionToolState : public GenericToolState
{
    public:
        SelectionToolState(ScenarioStateMachine& sm);

        void on_scenarioPressed() override;
        void on_scenarioMoved() override;
        void on_scenarioReleased() override;

};
