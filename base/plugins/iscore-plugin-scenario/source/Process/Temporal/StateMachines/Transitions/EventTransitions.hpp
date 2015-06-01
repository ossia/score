#pragma once
#include "Process/Temporal/StateMachines/ScenarioStateMachineBaseTransitions.hpp"

class ClickOnEvent_Transition : public MatchedScenarioTransition<ClickOnEvent_Event>
{
    public:
        using MatchedScenarioTransition::MatchedScenarioTransition;

    protected:
        void onTransition(QEvent * ev) override;
};

class MoveOnEvent_Transition : public MatchedScenarioTransition<MoveOnEvent_Event>
{
    public:
        using MatchedScenarioTransition::MatchedScenarioTransition;

    protected:
        void onTransition(QEvent * ev) override;
};

class ReleaseOnEvent_Transition : public MatchedScenarioTransition<ReleaseOnEvent_Event>
{
    public:
        using MatchedScenarioTransition::MatchedScenarioTransition;

    protected:
        void onTransition(QEvent * ev) override;
};
