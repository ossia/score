#pragma once
#include <Scenario/Process/Temporal/StateMachines/ScenarioStateMachineBaseTransitions.hpp>

template<>
class ScenarioTransition_T<ScenarioElement::Nothing + Modifier::Click_tag::value> final :
        public MatchedScenarioTransition<PositionedScenarioEvent<ScenarioElement::Nothing + Modifier::Click_tag::value>>
{
    public:
        using MatchedScenarioTransition::MatchedScenarioTransition;

    protected:
        void onTransition(QEvent * ev) override;
};
using ClickOnNothing_Transition = ScenarioTransition_T<ScenarioElement::Nothing + Modifier::Click_tag::value>;


template<>
class ScenarioTransition_T<ScenarioElement::Nothing + Modifier::Move_tag::value> final:
        public MatchedScenarioTransition<PositionedScenarioEvent<ScenarioElement::Nothing + Modifier::Move_tag::value>>
{
    public:
        using MatchedScenarioTransition::MatchedScenarioTransition;

    protected:
        void onTransition(QEvent * ev) override;
};
using MoveOnNothing_Transition = ScenarioTransition_T<ScenarioElement::Nothing + Modifier::Move_tag::value>;


template<>
class ScenarioTransition_T<ScenarioElement::Nothing + Modifier::Release_tag::value> final :
        public MatchedScenarioTransition<PositionedScenarioEvent<ScenarioElement::Nothing + Modifier::Release_tag::value>>
{
    public:
        using MatchedScenarioTransition::MatchedScenarioTransition;

    protected:
        void onTransition(QEvent * ev) override;
};
using ReleaseOnNothing_Transition = ScenarioTransition_T<ScenarioElement::Nothing + Modifier::Release_tag::value>;


