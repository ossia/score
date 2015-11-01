#pragma once
#include "ScenarioCreationState.hpp"

class ScenarioCreation_FromTimeNode final : public ScenarioCreationState
{
    public:
        ScenarioCreation_FromTimeNode(
                const ScenarioStateMachine& stateMachine,
                const Path<ScenarioModel>& scenarioPath,
                iscore::CommandStack& stack,
                QState* parent);

    private:
        void createInitialEventAndState();

        void createToNothing();
        void createToState();
        void createToEvent();
        void createToTimeNode();
};
