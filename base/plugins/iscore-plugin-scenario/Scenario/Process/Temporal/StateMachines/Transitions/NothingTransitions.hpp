#pragma once
#include <Scenario/Process/Temporal/StateMachines/ScenarioStateMachineBaseTransitions.hpp>

namespace Scenario
{
template<>
class Transition_T<ScenarioElement::Nothing + iscore::Modifier::Click_tag::value> final :
        public MatchedTransition<PositionedScenarioEvent<ScenarioElement::Nothing + iscore::Modifier::Click_tag::value>>
{
    public:
        using MatchedTransition::MatchedTransition;

    protected:
        void onTransition(QEvent * ev) override;
};
using ClickOnNothing_Transition = Transition_T<ScenarioElement::Nothing + iscore::Modifier::Click_tag::value>;


template<>
class Transition_T<ScenarioElement::Nothing + iscore::Modifier::Move_tag::value> final:
        public MatchedTransition<PositionedScenarioEvent<ScenarioElement::Nothing + iscore::Modifier::Move_tag::value>>
{
    public:
        using MatchedTransition::MatchedTransition;

    protected:
        void onTransition(QEvent * ev) override;
};
using MoveOnNothing_Transition = Transition_T<ScenarioElement::Nothing + iscore::Modifier::Move_tag::value>;


template<>
class Transition_T<ScenarioElement::Nothing + iscore::Modifier::Release_tag::value> final :
        public MatchedTransition<PositionedScenarioEvent<ScenarioElement::Nothing + iscore::Modifier::Release_tag::value>>
{
    public:
        using MatchedTransition::MatchedTransition;

    protected:
        void onTransition(QEvent * ev) override;
};
using ReleaseOnNothing_Transition = Transition_T<ScenarioElement::Nothing + iscore::Modifier::Release_tag::value>;
}
