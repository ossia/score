#pragma once

namespace Scenario
{
// SPace at the left of the main box in the main scenario view.
static const constexpr double ScenarioLeftSpace = 0.; // -5
static const constexpr double IntervalHeaderHeight = 21.;

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
    Comment = 1
    , TimeSync
    , Event
    , Interval
    , State
    , IntervalWithRack
    , SelectedInterval
    , SelectedTimeSync
    , SelectedEvent
    , SelectedState
  };
  enum IntervalItemZPos
  {
    Header = 1,
    Rack,
    Brace
  };
};
}
