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
        const ScenarioStateMachine& m_scenarioSM;
        void createToNothing();
        void createToTimeNode();
        void createToEvent();
        void createToState();

        bool creating{false};
};
