#pragma once
#include "ScenarioCreationState.hpp"

class ScenarioCreation_FromTimeNode : public ScenarioCreationState
{
    public:
        ScenarioCreation_FromTimeNode(
                const ScenarioStateMachine& stateMachine,
                ObjectPath&& scenarioPath,
                iscore::CommandStack& stack,
                QState* parent);

    private:
        void createSingleEventAndState();

        void createToNothing(const ScenarioStateMachine& stateMachine);
        void createToState();
        void createToEvent();
        void createToTimeNode();
};
