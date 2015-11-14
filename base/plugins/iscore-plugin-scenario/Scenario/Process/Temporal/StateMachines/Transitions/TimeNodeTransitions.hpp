#pragma once
#include <Scenario/Process/Temporal/StateMachines/ScenarioStateMachineBaseTransitions.hpp>

namespace Scenario
{
template<>
class Transition_T<ScenarioElement::TimeNode +  iscore::Modifier::Click_tag::value> final :
        public MatchedTransition<PositionedWithId_ScenarioEvent<TimeNodeModel, ScenarioElement::TimeNode +  iscore::Modifier::Click_tag::value>>
{
    public:
        using MatchedTransition::MatchedTransition;

    protected:
        void onTransition(QEvent * ev) override;
};
using ClickOnTimeNode_Transition = Transition_T<ScenarioElement::TimeNode +  iscore::Modifier::Click_tag::value>;


template<>
class Transition_T<ScenarioElement::TimeNode +  iscore::Modifier::Move_tag::value> final :
        public MatchedTransition<PositionedWithId_ScenarioEvent<TimeNodeModel, ScenarioElement::TimeNode +  iscore::Modifier::Move_tag::value>>
{
    public:
        using MatchedTransition::MatchedTransition;

    protected:
        void onTransition(QEvent * ev) override;
};
using MoveOnTimeNode_Transition = Transition_T<ScenarioElement::TimeNode +  iscore::Modifier::Move_tag::value>;


template<>
class Transition_T<ScenarioElement::TimeNode +  iscore::Modifier::Release_tag::value> final :
        public MatchedTransition<PositionedWithId_ScenarioEvent<TimeNodeModel, ScenarioElement::TimeNode +  iscore::Modifier::Release_tag::value>>
{
    public:
        using MatchedTransition::MatchedTransition;

    protected:
        void onTransition(QEvent * ev) override;
};
using ReleaseOnTimeNode_Transition = Transition_T<ScenarioElement::TimeNode +  iscore::Modifier::Release_tag::value>;

}
