#pragma once
#include "ScenarioCreationState.hpp"

class ScenarioCreation_FromState : public ScenarioCreationState
{
    public:
        ScenarioCreation_FromState(
                const ScenarioStateMachine& stateMachine,
                ObjectPath&& scenarioPath,
                iscore::CommandStack& stack,
                QState* parent);

    private:
        void createSingleEventOnTimeNode();

        void createEventFromStateOnNothing(const ScenarioStateMachine& stateMachine);
        void createEventFromStateOnTimeNode();

        void createConstraintBetweenEvents();
};
