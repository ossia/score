#pragma once
#include "ScenarioCreationState.hpp"

class ScenarioCreation_FromTimeNode : public ScenarioCreationState
{
    public:
        ScenarioCreation_FromTimeNode(
                const ScenarioStateMachine& stateMachine,
                const ModelPath<ScenarioModel>& scenarioPath,
                iscore::CommandStack& stack,
                QState* parent);

    private:
        void createInitialEventAndState();

        void createToNothing();
        void createToState();
        void createToEvent();
        void createToTimeNode();
};
