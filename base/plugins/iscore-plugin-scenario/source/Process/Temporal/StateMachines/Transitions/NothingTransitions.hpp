#pragma once
#include "Process/Temporal/StateMachines/ScenarioStateMachineBaseTransitions.hpp"

class ClickOnNothing_Transition : public MatchedScenarioTransition<ClickOnNothing_Event>
{
    public:
        using MatchedScenarioTransition::MatchedScenarioTransition;

    protected:
        void onTransition(QEvent * ev) override;
};

class MoveOnNothing_Transition : public MatchedScenarioTransition<MoveOnNothing_Event>
{
    public:
        using MatchedScenarioTransition::MatchedScenarioTransition;

    protected:
        void onTransition(QEvent * ev) override;
};

class ReleaseOnNothing_Transition : public MatchedScenarioTransition<ReleaseOnNothing_Event>
{
    public:
        using MatchedScenarioTransition::MatchedScenarioTransition;

    protected:
        virtual void onTransition(QEvent * ev);
};
