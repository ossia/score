#pragma once
#include "ScenarioStateMachineBaseEvents.hpp"
#include "ScenarioStateMachineBaseStates.hpp"

namespace Scenario
{
template<typename T>
using GenericTransition = iscore::StateAwareTransition<Scenario::StateBase, T>;

template<typename Event_T>
class MatchedTransition : public Scenario::GenericTransition<iscore::MatchedTransition<Event_T>>
{
    public:
        using Scenario::GenericTransition<iscore::MatchedTransition<Event_T>>::GenericTransition;
};

template<int Value>
class Transition_T;
}
