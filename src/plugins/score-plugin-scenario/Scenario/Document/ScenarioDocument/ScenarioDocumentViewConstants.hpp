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
    Type = 1,
    UserType = 65536, // See QGraphicsItem
    Interval,
    LeftBrace,
    RightBrace,
    SlotHeader,
    SlotFooter,
    SlotOverlay,
    IntervalHeader,
    TimeSync,
    Trigger,
    Event,
    State,
    Comment,
    Condition,
    StateOverlay,
    GraphInterval,

    SlotFooterDelegate = UserType + 10000
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
    State,
    IntervalWithRack,
    SelectedInterval,
    SelectedTimeSync,
    SelectedEvent,
    SelectedState
  };
  enum IntervalItemZPos
  {
    Header = 1,
    Rack,
    Brace
  };
};
}
