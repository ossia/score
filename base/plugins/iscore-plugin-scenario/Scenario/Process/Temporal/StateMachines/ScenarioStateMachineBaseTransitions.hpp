#pragma once
#include "ScenarioStateMachineBaseEvents.hpp"
#include "ScenarioStateMachineBaseStates.hpp"

template<typename T>
using GenericScenarioTransition = StateAwareTransition<ScenarioStateBase, T>;

template<typename Event>
class MatchedScenarioTransition : public GenericScenarioTransition<MatchedTransition<Event>>
{
    public:
        using GenericScenarioTransition<MatchedTransition<Event>>::GenericScenarioTransition;
};

template<int Value>
class ScenarioTransition_T;
