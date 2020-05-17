#pragma once
#include "ScenarioPaletteBaseEvents.hpp"
#include "ScenarioPaletteBaseStates.hpp"

namespace Scenario
{
template <typename Scenario_T, typename T>
using GenericTransition = score::StateAwareTransition<Scenario::StateBase<Scenario_T>, T>;

template <typename Scenario_T, typename Event_T>
class MatchedTransition
    : public score::
          StateAwareTransition<Scenario::StateBase<Scenario_T>, score::MatchedTransition<Event_T>>
{
public:
  using score::StateAwareTransition<
      Scenario::StateBase<Scenario_T>,
      score::MatchedTransition<Event_T>>::StateAwareTransition;
};

template <typename Scenario_T, int Value>
class Transition_T;
}
