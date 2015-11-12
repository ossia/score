#pragma once
#include <Scenario/Process/Temporal/StateMachines/ScenarioStateMachineBaseTransitions.hpp>

template<>
class ScenarioTransition_T<ScenarioElement::State + Modifier::Click_tag::value> final :
        public MatchedScenarioTransition<PositionedWithId_ScenarioEvent<StateModel, ScenarioElement::State + Modifier::Click_tag::value>>
{
    public:
        using MatchedScenarioTransition::MatchedScenarioTransition;

    protected:
        void onTransition(QEvent * ev) override;
};
using ClickOnState_Transition = ScenarioTransition_T<ScenarioElement::State + Modifier::Click_tag::value>;


template<>
class ScenarioTransition_T<ScenarioElement::State + Modifier::Move_tag::value> final :
        public MatchedScenarioTransition<PositionedWithId_ScenarioEvent<StateModel, ScenarioElement::State + Modifier::Move_tag::value>>
{
    public:
        using MatchedScenarioTransition::MatchedScenarioTransition;

    protected:
        void onTransition(QEvent * ev) override;
};
using MoveOnState_Transition = ScenarioTransition_T<ScenarioElement::State + Modifier::Move_tag::value>;


template<>
class ScenarioTransition_T<ScenarioElement::State + Modifier::Release_tag::value> final :
        public MatchedScenarioTransition<PositionedWithId_ScenarioEvent<StateModel, ScenarioElement::State + Modifier::Release_tag::value>>
{
    public:
        using MatchedScenarioTransition::MatchedScenarioTransition;

    protected:
        void onTransition(QEvent * ev) override;
};
using ReleaseOnState_Transition = ScenarioTransition_T<ScenarioElement::State + Modifier::Release_tag::value>;
