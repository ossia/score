#pragma once
#include "ScenarioStateMachineBaseEvents.hpp"
#include "ScenarioStateMachineBaseStates.hpp"

// TODO rename in something like CreateMove transition...
// Or something that means that it will operate on constraint, event, etc...
// TODO put in scenario namespace.

template<typename T>
using GenericScenarioTransition = StateAwareTransition<ScenarioStateBase, T>;

template<typename Event>
class MatchedScenarioTransition : public GenericScenarioTransition<MatchedTransition<Event>>
{
    public:
        using GenericScenarioTransition<MatchedTransition<Event>>::GenericScenarioTransition;
};
