#pragma once
#include <Scenario/Process/Temporal/StateMachines/ScenarioStateMachineBaseTransitions.hpp>

namespace Scenario
{
template<>
class Transition_T<ScenarioElement::Event +  iscore::Modifier::Click_tag::value> final :
        public MatchedTransition<PositionedWithId_ScenarioEvent<EventModel, ScenarioElement::Event +  iscore::Modifier::Click_tag::value>>
{
    public:
        using MatchedTransition::MatchedTransition;

    protected:
        void onTransition(QEvent * ev) override;
};
using ClickOnEvent_Transition = Transition_T<ScenarioElement::Event +  iscore::Modifier::Click_tag::value>;


template<>
class Transition_T<ScenarioElement::Event +  iscore::Modifier::Move_tag::value> final :
        public MatchedTransition<PositionedWithId_ScenarioEvent<EventModel, ScenarioElement::Event +  iscore::Modifier::Move_tag::value>>
{
    public:
        using MatchedTransition::MatchedTransition;

    protected:
        void onTransition(QEvent * ev) override;
};
using MoveOnEvent_Transition = Transition_T<ScenarioElement::Event +  iscore::Modifier::Move_tag::value>;


template<>
class Transition_T<ScenarioElement::Event +  iscore::Modifier::Release_tag::value> final :
        public MatchedTransition<PositionedWithId_ScenarioEvent<EventModel, ScenarioElement::Event +  iscore::Modifier::Release_tag::value>>
{
    public:
        using MatchedTransition::MatchedTransition;

    protected:
        void onTransition(QEvent * ev) override;
};
using ReleaseOnEvent_Transition = Transition_T<ScenarioElement::Event +  iscore::Modifier::Release_tag::value>;
}
