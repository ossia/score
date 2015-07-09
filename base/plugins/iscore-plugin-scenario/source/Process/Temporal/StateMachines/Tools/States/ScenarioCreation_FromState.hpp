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
        void createToNothing();
        void createToTimeNode();
        void createToEvent();
        void createToState();

        template<typename Fun>
        void creationCheck(Fun&& fun);
};
