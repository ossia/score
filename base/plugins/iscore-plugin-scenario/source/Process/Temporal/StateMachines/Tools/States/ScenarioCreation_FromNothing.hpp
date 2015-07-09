#pragma once
#include "ScenarioCreationState.hpp"

class ScenarioCreation_FromNothing : public ScenarioCreationState
{
    public:
        ScenarioCreation_FromNothing(
                const ScenarioStateMachine& stateMachine,
                ObjectPath&& scenarioPath,
                iscore::CommandStack& stack,
                QState* parent);

    private:
        void createInitialState();

        void createToNothing();
        void createToState();
        void createToEvent();
        void createToTimeNode();
};
