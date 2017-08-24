#pragma once

namespace Scenario
{
// SPace at the left of the main box in the main scenario view.
static const constexpr double ScenarioLeftSpace = 0.; // -5
static const constexpr double ConstraintHeaderHeight = 30.;

class ItemType
{
public:
  enum Type
  {
    Constraint = 1,
    LeftBrace,
    RightBrace,
    SlotHandle,
    SlotOverlay,
    ConstraintHeader,
    TimeSync,
    Trigger,
    Event,
    State,
    Comment,
    Condition
  };
};

class ZPos
{
public:
  enum ItemZPos
  {
    Comment = 1,
    TimeSync,
    Event,
    Constraint,
    SelectedConstraint,
    State
  };
  enum ConstraintItemZPos
  {
    Header = 1,
    Rack,
    Brace
  };
};
}
