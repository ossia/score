#pragma once

namespace Scenario
{
// SPace at the left of the main box in the main scenario view.
static const constexpr double ScenarioLeftSpace = 0.; // -5
static const constexpr double IntervalHeaderHeight = 30.;

class ItemType
{
public:
  enum Type
  {
    Interval = 1,
    LeftBrace,
    RightBrace,
    SlotHandle,
    SlotHeader,
    SlotOverlay,
    IntervalHeader,
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
    Interval,
    SelectedInterval,
    State
  };
  enum IntervalItemZPos
  {
    Header = 1,
    Rack,
    Brace
  };
};
}
