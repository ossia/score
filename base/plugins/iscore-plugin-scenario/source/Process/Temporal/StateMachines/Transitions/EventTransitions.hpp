#pragma once
#include "Process/Temporal/StateMachines/ScenarioStateMachineBaseTransitions.hpp"


template<>
class ScenarioTransition_T<ScenarioElement::Event + Modifier::Click_tag::value> :
        public MatchedScenarioTransition<PositionedWithId_ScenarioEvent<EventModel, ScenarioElement::Event + Modifier::Click_tag::value>>
{
    public:
        using MatchedScenarioTransition::MatchedScenarioTransition;

    protected:
        void onTransition(QEvent * ev) override;
};
using ClickOnEvent_Transition = ScenarioTransition_T<ScenarioElement::Event + Modifier::Click_tag::value>;


template<>
class ScenarioTransition_T<ScenarioElement::Event + Modifier::Move_tag::value> :
        public MatchedScenarioTransition<PositionedWithId_ScenarioEvent<EventModel, ScenarioElement::Event + Modifier::Move_tag::value>>
{
    public:
        using MatchedScenarioTransition::MatchedScenarioTransition;

    protected:
        void onTransition(QEvent * ev) override;
};
using MoveOnEvent_Transition = ScenarioTransition_T<ScenarioElement::Event + Modifier::Move_tag::value>;


template<>
class ScenarioTransition_T<ScenarioElement::Event + Modifier::Release_tag::value> :
        public MatchedScenarioTransition<PositionedWithId_ScenarioEvent<EventModel, ScenarioElement::Event + Modifier::Release_tag::value>>
{
    public:
        using MatchedScenarioTransition::MatchedScenarioTransition;

    protected:
        virtual void onTransition(QEvent * ev);
};
using ReleaseOnEvent_Transition = ScenarioTransition_T<ScenarioElement::Event + Modifier::Release_tag::value>;


