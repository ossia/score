#pragma once
#include "ScenarioCreationState.hpp"

class ScenarioCreation_FromEvent : public ScenarioCreationState
{
    public:
        ScenarioCreation_FromEvent(
                const ScenarioStateMachine& stateMachine,
                const Path<ScenarioModel>& scenarioPath,
                iscore::CommandStack& stack,
                QState* parent);

    private:
        void createInitialState();

        void createToNothing();
        void createToState();
        void createToEvent();
        void createToTimeNode();
};
