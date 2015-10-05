#pragma once
#include "Process/Temporal/StateMachines/ScenarioStateMachineBaseTransitions.hpp"

template<>
class ScenarioTransition_T<ScenarioElement::TimeNode + Modifier::Click_tag::value> final :
        public MatchedScenarioTransition<PositionedWithId_ScenarioEvent<TimeNodeModel, ScenarioElement::TimeNode + Modifier::Click_tag::value>>
{
    public:
        using MatchedScenarioTransition::MatchedScenarioTransition;

    protected:
        void onTransition(QEvent * ev) override;
};
using ClickOnTimeNode_Transition = ScenarioTransition_T<ScenarioElement::TimeNode + Modifier::Click_tag::value>;


template<>
class ScenarioTransition_T<ScenarioElement::TimeNode + Modifier::Move_tag::value> final :
        public MatchedScenarioTransition<PositionedWithId_ScenarioEvent<TimeNodeModel, ScenarioElement::TimeNode + Modifier::Move_tag::value>>
{
    public:
        using MatchedScenarioTransition::MatchedScenarioTransition;

    protected:
        void onTransition(QEvent * ev) override;
};
using MoveOnTimeNode_Transition = ScenarioTransition_T<ScenarioElement::TimeNode + Modifier::Move_tag::value>;


template<>
class ScenarioTransition_T<ScenarioElement::TimeNode + Modifier::Release_tag::value> final :
        public MatchedScenarioTransition<PositionedWithId_ScenarioEvent<TimeNodeModel, ScenarioElement::TimeNode + Modifier::Release_tag::value>>
{
    public:
        using MatchedScenarioTransition::MatchedScenarioTransition;

    protected:
        virtual void onTransition(QEvent * ev) override;
};
using ReleaseOnTimeNode_Transition = ScenarioTransition_T<ScenarioElement::TimeNode + Modifier::Release_tag::value>;

