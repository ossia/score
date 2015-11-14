#pragma once
#include <Scenario/Process/Temporal/StateMachines/ScenarioStateMachineBaseTransitions.hpp>

namespace Scenario
{
template<>
class Transition_T<ScenarioElement::State +  iscore::Modifier::Click_tag::value> final :
        public MatchedTransition<PositionedWithId_ScenarioEvent<StateModel, ScenarioElement::State +  iscore::Modifier::Click_tag::value>>
{
    public:
        using MatchedTransition::MatchedTransition;

    protected:
        void onTransition(QEvent * ev) override;
};
using ClickOnState_Transition = Transition_T<ScenarioElement::State +  iscore::Modifier::Click_tag::value>;


template<>
class Transition_T<ScenarioElement::State +  iscore::Modifier::Move_tag::value> final :
        public MatchedTransition<PositionedWithId_ScenarioEvent<StateModel, ScenarioElement::State +  iscore::Modifier::Move_tag::value>>
{
    public:
        using MatchedTransition::MatchedTransition;

    protected:
        void onTransition(QEvent * ev) override;
};
using MoveOnState_Transition = Transition_T<ScenarioElement::State +  iscore::Modifier::Move_tag::value>;


template<>
class Transition_T<ScenarioElement::State +  iscore::Modifier::Release_tag::value> final :
        public MatchedTransition<PositionedWithId_ScenarioEvent<StateModel, ScenarioElement::State +  iscore::Modifier::Release_tag::value>>
{
    public:
        using MatchedTransition::MatchedTransition;

    protected:
        void onTransition(QEvent * ev) override;
};
using ReleaseOnState_Transition = Transition_T<ScenarioElement::State +  iscore::Modifier::Release_tag::value>;
}
