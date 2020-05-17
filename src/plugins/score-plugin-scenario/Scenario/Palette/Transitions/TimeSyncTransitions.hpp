#pragma once
#include <Scenario/Palette/ScenarioPaletteBaseTransitions.hpp>
#include <Scenario/Process/ScenarioInterface.hpp>
namespace Scenario
{
template <typename Scenario_T>
class Transition_T<Scenario_T, ClickOnTimeSync> final
    : public MatchedTransition<Scenario_T, ClickOnTimeSync_Event>
{
public:
  using MatchedTransition<Scenario_T, ClickOnTimeSync_Event>::MatchedTransition;

protected:
  void onTransition(QEvent* ev) override
  {
    auto qev = static_cast<ClickOnTimeSync_Event*>(ev);
    this->state().clear();

    this->state().clickedTimeSync = qev->id;
    this->state().currentPoint = qev->point;
  }
};
template <typename Scenario_T>
using ClickOnTimeSync_Transition = Transition_T<Scenario_T, ClickOnTimeSync>;

template <typename Scenario_T>
class ClickOnEndTimeSync_Transition final
    : public MatchedTransition<Scenario_T, ClickOnTimeSync_Event>
{
public:
  using MatchedTransition<Scenario_T, ClickOnTimeSync_Event>::MatchedTransition;

protected:
  bool eventTest(QEvent* e) override
  {
    if (e->type() == QEvent::Type(QEvent::User + ClickOnTimeSync_Event::user_type))
    {
      auto qev = static_cast<ClickOnTimeSync_Event*>(e);
      return qev->id == Scenario::endId<TimeSyncModel>();
    }
    return false;
  }

  void onTransition(QEvent* ev) override
  {
    auto qev = static_cast<ClickOnTimeSync_Event*>(ev);
    this->state().clear();

    this->state().clickedTimeSync = qev->id;
    this->state().currentPoint = qev->point;
  }
};

template <typename Scenario_T>
class Transition_T<Scenario_T, MoveOnTimeSync> final
    : public MatchedTransition<Scenario_T, MoveOnTimeSync_Event>
{
public:
  using MatchedTransition<Scenario_T, MoveOnTimeSync_Event>::MatchedTransition;

protected:
  void onTransition(QEvent* ev) override
  {
    auto qev = static_cast<MoveOnTimeSync_Event*>(ev);

    this->state().hoveredTimeSync = qev->id;
    this->state().currentPoint = qev->point;
  }
};
template <typename Scenario_T>
using MoveOnTimeSync_Transition = Transition_T<Scenario_T, MoveOnTimeSync>;

template <typename Scenario_T>
class Transition_T<Scenario_T, ReleaseOnTimeSync> final
    : public MatchedTransition<Scenario_T, ReleaseOnTimeSync_Event>
{
public:
  using MatchedTransition<Scenario_T, ReleaseOnTimeSync_Event>::MatchedTransition;

protected:
  void onTransition(QEvent* ev) override
  {
    auto qev = static_cast<ReleaseOnTimeSync_Event*>(ev);

    this->state().hoveredTimeSync = qev->id;
    this->state().currentPoint = qev->point;
  }
};
template <typename Scenario_T>
using ReleaseOnTimeSync_Transition = Transition_T<Scenario_T, ReleaseOnTimeSync>;
}
