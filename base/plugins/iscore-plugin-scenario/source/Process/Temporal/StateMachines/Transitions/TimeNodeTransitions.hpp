#pragma once
#include "Process/Temporal/StateMachines/ScenarioStateMachineBaseTransitions.hpp"

class ClickOnTimeNode_Transition : public MatchedScenarioTransition<ClickOnTimeNode_Event>
{
    public:
        using MatchedScenarioTransition::MatchedScenarioTransition;

    protected:
        void onTransition(QEvent * ev) override;
};

class MoveOnTimeNode_Transition : public MatchedScenarioTransition<MoveOnTimeNode_Event>
{
    public:
        using MatchedScenarioTransition::MatchedScenarioTransition;

    protected:
        void onTransition(QEvent * ev) override;
};


class ReleaseOnTimeNode_Transition : public MatchedScenarioTransition<ReleaseOnTimeNode_Event>
{
    public:
        using MatchedScenarioTransition::MatchedScenarioTransition;

    protected:
        void onTransition(QEvent * ev) override;
};
