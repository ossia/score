#pragma once
#include "GenericToolState.hpp"


class CreationToolState : public GenericToolState
{
    public:
        CreationToolState(ScenarioStateMachine& sm);

        void on_scenarioPressed() override;
        void on_scenarioMoved() override;
        void on_scenarioReleased() override;
};
