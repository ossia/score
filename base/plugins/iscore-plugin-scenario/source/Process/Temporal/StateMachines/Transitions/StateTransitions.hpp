#pragma once
#include "Process/Temporal/StateMachines/ScenarioStateMachineBaseTransitions.hpp"

class ClickOnState_Transition : public MatchedScenarioTransition<ClickOnState_Event>
{
    public:
	using MatchedScenarioTransition::MatchedScenarioTransition;

    protected:
	void onTransition(QEvent * ev) override;
};

class MoveOnState_Transition : public MatchedScenarioTransition<MoveOnState_Event>
{
    public:
	using MatchedScenarioTransition::MatchedScenarioTransition;

    protected:
	void onTransition(QEvent * ev) override;
};

class ReleaseOnState_Transition : public MatchedScenarioTransition<ReleaseOnState_Event>
{
    public:
	using MatchedScenarioTransition::MatchedScenarioTransition;

    protected:
	void onTransition(QEvent * ev) override;
};
