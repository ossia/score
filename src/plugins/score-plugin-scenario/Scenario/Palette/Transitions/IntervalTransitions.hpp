#pragma once
#include <Scenario/Palette/ScenarioPaletteBaseTransitions.hpp>

#include <QEvent>

namespace Scenario
{
template <typename Scenario_T>
class Transition_T<Scenario_T, ClickOnInterval> final
    : public MatchedTransition<Scenario_T, ClickOnInterval_Event>
{
public:
  using MatchedTransition<Scenario_T, ClickOnInterval_Event>::MatchedTransition;

protected:
  void onTransition(QEvent* ev) override
  {
    auto qev = static_cast<ClickOnInterval_Event*>(ev);
    this->state().clear();

    this->state().clickedInterval = qev->id;
    this->state().currentPoint = qev->point;
  }
};
template <typename Scenario_T>
using ClickOnInterval_Transition = Transition_T<Scenario_T, ClickOnInterval>;

template <typename Scenario_T>
class Transition_T<Scenario_T, MoveOnInterval> final
    : public MatchedTransition<Scenario_T, MoveOnInterval_Event>
{
public:
  using MatchedTransition<Scenario_T, MoveOnInterval_Event>::MatchedTransition;

protected:
  void onTransition(QEvent* ev) override
  {
    auto qev = static_cast<MoveOnInterval_Event*>(ev);

    this->state().hoveredInterval = qev->id;
    this->state().currentPoint = qev->point;
  }
};
template <typename Scenario_T>
using MoveOnInterval_Transition = Transition_T<Scenario_T, MoveOnInterval>;

template <typename Scenario_T>
class Transition_T<Scenario_T, ClickOnLeftBrace> final
    : public MatchedTransition<Scenario_T, ClickOnLeftBrace_Event>
{
public:
  using MatchedTransition<Scenario_T, ClickOnLeftBrace_Event>::MatchedTransition;

protected:
  void onTransition(QEvent* ev) override
  {
    auto qev = static_cast<ClickOnLeftBrace_Event*>(ev);
    this->state().clear();

    this->state().clickedInterval = qev->id;
    this->state().currentPoint = qev->point;
  }
};
template <typename Scenario_T>
using ClickOnLeftBrace_Transition = Transition_T<Scenario_T, ClickOnLeftBrace>;

template <typename Scenario_T>
class Transition_T<Scenario_T, ClickOnRightBrace> final
    : public MatchedTransition<Scenario_T, ClickOnRightBrace_Event>
{
public:
  using MatchedTransition<Scenario_T, ClickOnRightBrace_Event>::MatchedTransition;

protected:
  void onTransition(QEvent* ev) override
  {
    auto qev = static_cast<ClickOnRightBrace_Event*>(ev);
    this->state().clear();

    this->state().clickedInterval = qev->id;
    this->state().currentPoint = qev->point;
  }
};
template <typename Scenario_T>
using ClickOnRightBrace_Transition = Transition_T<Scenario_T, ClickOnRightBrace>;

} // namespace Scenario
