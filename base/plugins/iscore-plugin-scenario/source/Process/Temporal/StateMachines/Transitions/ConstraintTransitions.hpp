#pragma once
#include "Process/Temporal/StateMachines/ScenarioStateMachineBaseTransitions.hpp"

class ClickOnConstraint_Transition : public MatchedScenarioTransition<ClickOnConstraint_Event>
{
    public:
        using MatchedScenarioTransition::MatchedScenarioTransition;

    protected:
        void onTransition(QEvent * ev) override;
};

class MoveOnConstraint_Transition : public MatchedScenarioTransition<MoveOnConstraint_Event>
{
    public:
        using MatchedScenarioTransition::MatchedScenarioTransition;

    protected:
        virtual void onTransition(QEvent * ev) override;
};
