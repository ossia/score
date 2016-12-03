#pragma once
#include "ScenarioPaletteBaseEvents.hpp"
#include "ScenarioPaletteBaseStates.hpp"

namespace Scenario
{
template <typename Scenario_T, typename T>
using GenericTransition
    = iscore::StateAwareTransition<Scenario::StateBase<Scenario_T>, T>;

template <typename Scenario_T, typename Event_T>
class MatchedTransition
    : public iscore::
          StateAwareTransition<Scenario::StateBase<Scenario_T>, iscore::MatchedTransition<Event_T>>
{
public:
  using iscore::
      StateAwareTransition<Scenario::StateBase<Scenario_T>, iscore::MatchedTransition<Event_T>>::
          StateAwareTransition;
};

template <typename Scenario_T, int Value>
class Transition_T;
}
