#pragma once
#include <Scenario/Process/Temporal/StateMachines/ScenarioStateMachineBaseTransitions.hpp>

namespace Scenario
{
template<>
class Transition_T<ScenarioElement::Constraint +  iscore::Modifier::Click_tag::value> final :
        public MatchedTransition<PositionedWithId_ScenarioEvent<ConstraintModel, ScenarioElement::Constraint +  iscore::Modifier::Click_tag::value>>
{
    public:
        using MatchedTransition::MatchedTransition;

    protected:
        void onTransition(QEvent * ev) override;
};
using ClickOnConstraint_Transition = Transition_T<ScenarioElement::Constraint +  iscore::Modifier::Click_tag::value>;


template<>
class Transition_T<ScenarioElement::Constraint +  iscore::Modifier::Move_tag::value> final :
        public MatchedTransition<PositionedWithId_ScenarioEvent<ConstraintModel, ScenarioElement::Constraint +  iscore::Modifier::Move_tag::value>>
{
    public:
        using MatchedTransition::MatchedTransition;

    protected:
        void onTransition(QEvent * ev) override;
};
using MoveOnConstraint_Transition = Transition_T<ScenarioElement::Constraint +  iscore::Modifier::Move_tag::value>;
}
