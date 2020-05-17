#pragma once
#include "ScenarioPoint.hpp"

#include <Scenario/Document/Interval/Slot.hpp>

#include <score/statemachine/StateMachineUtils.hpp>
#include <score/tools/Clamp.hpp>

namespace score
{
template <>
struct PositionedEvent<Scenario::Point> : public QEvent
{
  PositionedEvent(const Scenario::Point& pt, QEvent::Type type) : QEvent{type}, point(pt)
  {
    // Here we artificially prevent to move over the header of the rack
    // so that the elements won't disappear in the void.
    point.y = clamp(point.y, 0.004, 0.99);
  }

  ~PositionedEvent() override = default;

  Scenario::Point point;
};
}

namespace Scenario
{
class TimeSyncModel;
class EventModel;
class IntervalModel;
class StateModel;
class TriggerModel;

// We avoid virtual inheritance (with Numbered event);
// this replicates a tiny bit of code.
template <int N>
struct PositionedScenarioEvent : public score::PositionedEvent<Scenario::Point>
{
  static constexpr const int user_type = N;
  PositionedScenarioEvent(const Scenario::Point& pt)
      : PositionedEvent<Scenario::Point>{pt, QEvent::Type(QEvent::User + N)}
  {
  }
};

template <typename Element, int N>
struct PositionedWithId_ScenarioEvent final : public PositionedScenarioEvent<N>
{
  PositionedWithId_ScenarioEvent(const Id<Element>& tn_id, const Scenario::Point& sp)
      : PositionedScenarioEvent<N>{sp}, id{tn_id}
  {
  }

  Id<Element> id;
};

////////////
// Events
enum ScenarioElement
{
  Nothing,
  TimeSync,
  Event,
  Interval,
  State,
  SlotOverlay_e,
  SlotHandle_e,
  Trigger,
  LeftBrace,
  RightBrace
};

static const constexpr int ClickOnNothing
    = ScenarioElement::Nothing + score::Modifier::Click_tag::value;
static const constexpr int ClickOnTimeSync
    = ScenarioElement::TimeSync + score::Modifier::Click_tag::value;
static const constexpr int ClickOnEvent
    = ScenarioElement::Event + score::Modifier::Click_tag::value;
static const constexpr int ClickOnInterval
    = ScenarioElement::Interval + score::Modifier::Click_tag::value;
static const constexpr int ClickOnState
    = ScenarioElement::State + score::Modifier::Click_tag::value;
static const constexpr int ClickOnSlotHandle
    = ScenarioElement::SlotHandle_e + score::Modifier::Click_tag::value;
static const constexpr int ClickOnTrigger
    = ScenarioElement::Trigger + score::Modifier::Click_tag::value;
static const constexpr int ClickOnLeftBrace
    = ScenarioElement::LeftBrace + score::Modifier::Click_tag::value;
static const constexpr int ClickOnRightBrace
    = ScenarioElement::RightBrace + score::Modifier::Click_tag::value;

static const constexpr int MoveOnNothing
    = ScenarioElement::Nothing + score::Modifier::Move_tag::value;
static const constexpr int MoveOnTimeSync
    = ScenarioElement::TimeSync + score::Modifier::Move_tag::value;
static const constexpr int MoveOnEvent = ScenarioElement::Event + score::Modifier::Move_tag::value;
static const constexpr int MoveOnInterval
    = ScenarioElement::Interval + score::Modifier::Move_tag::value;
static const constexpr int MoveOnState = ScenarioElement::State + score::Modifier::Move_tag::value;
static const constexpr int MoveOnSlotHandle
    = ScenarioElement::SlotHandle_e + score::Modifier::Move_tag::value;
static const constexpr int MoveOnTrigger
    = ScenarioElement::Trigger + score::Modifier::Move_tag::value;
static const constexpr int MoveOnLeftBrace
    = ScenarioElement::LeftBrace + score::Modifier::Move_tag::value;
static const constexpr int MoveOnRightBrace
    = ScenarioElement::RightBrace + score::Modifier::Move_tag::value;

static const constexpr int ReleaseOnNothing
    = ScenarioElement::Nothing + score::Modifier::Release_tag::value;
static const constexpr int ReleaseOnTimeSync
    = ScenarioElement::TimeSync + score::Modifier::Release_tag::value;
static const constexpr int ReleaseOnEvent
    = ScenarioElement::Event + score::Modifier::Release_tag::value;
static const constexpr int ReleaseOnInterval
    = ScenarioElement::Interval + score::Modifier::Release_tag::value;
static const constexpr int ReleaseOnState
    = ScenarioElement::State + score::Modifier::Release_tag::value;
static const constexpr int ReleaseOnSlotHandle
    = ScenarioElement::SlotHandle_e + score::Modifier::Release_tag::value;
static const constexpr int ReleaseOnTrigger
    = ScenarioElement::Trigger + score::Modifier::Release_tag::value;
static const constexpr int ReleaseOnLeftBrace
    = ScenarioElement::LeftBrace + score::Modifier::Release_tag::value;
static const constexpr int ReleaseOnRightBrace
    = ScenarioElement::RightBrace + score::Modifier::Release_tag::value;

/* click */
using ClickOnNothing_Event = PositionedScenarioEvent<ClickOnNothing>;
using ClickOnTimeSync_Event = PositionedWithId_ScenarioEvent<TimeSyncModel, ClickOnTimeSync>;
using ClickOnEvent_Event = PositionedWithId_ScenarioEvent<EventModel, ClickOnEvent>;
using ClickOnInterval_Event = PositionedWithId_ScenarioEvent<IntervalModel, ClickOnInterval>;
using ClickOnState_Event = PositionedWithId_ScenarioEvent<StateModel, ClickOnState>;
using ClickOnTrigger_Event = score::NumberedWithPath_Event<TriggerModel, ClickOnTrigger>;
using ClickOnLeftBrace_Event = PositionedWithId_ScenarioEvent<IntervalModel, ClickOnLeftBrace>;
using ClickOnRightBrace_Event = PositionedWithId_ScenarioEvent<IntervalModel, ClickOnRightBrace>;

/* move on */
using MoveOnNothing_Event = PositionedScenarioEvent<MoveOnNothing>;
using MoveOnTimeSync_Event = PositionedWithId_ScenarioEvent<TimeSyncModel, MoveOnTimeSync>;
using MoveOnEvent_Event = PositionedWithId_ScenarioEvent<EventModel, MoveOnEvent>;
using MoveOnInterval_Event = PositionedWithId_ScenarioEvent<IntervalModel, MoveOnInterval>;
using MoveOnState_Event = PositionedWithId_ScenarioEvent<StateModel, MoveOnState>;
using MoveOnTrigger_Event = score::NumberedWithPath_Event<TriggerModel, MoveOnTrigger>;
using MoveOnLeftBrace_Event = PositionedWithId_ScenarioEvent<IntervalModel, MoveOnLeftBrace>;
using MoveOnRightBrace_Event = PositionedWithId_ScenarioEvent<IntervalModel, MoveOnRightBrace>;

/* release on */
using ReleaseOnNothing_Event = PositionedScenarioEvent<ReleaseOnNothing>;
using ReleaseOnTimeSync_Event = PositionedWithId_ScenarioEvent<TimeSyncModel, ReleaseOnTimeSync>;
using ReleaseOnEvent_Event = PositionedWithId_ScenarioEvent<EventModel, ReleaseOnEvent>;
using ReleaseOnInterval_Event = PositionedWithId_ScenarioEvent<IntervalModel, ReleaseOnInterval>;
using ReleaseOnState_Event = PositionedWithId_ScenarioEvent<StateModel, ReleaseOnState>;
using ReleaseOnTrigger_Event = score::NumberedWithPath_Event<TriggerModel, ReleaseOnTrigger>;
using ReleaseOnLeftBrace_Event = PositionedWithId_ScenarioEvent<IntervalModel, ReleaseOnLeftBrace>;
using ReleaseOnRightBrace_Event
    = PositionedWithId_ScenarioEvent<IntervalModel, ReleaseOnRightBrace>;

// using ReleaseOnSlotHandle_Event
//    = score::NumberedWithPath_Event<IntervalModel, ReleaseOnSlotHandle>;
// using MoveOnSlotHandle_Event
//    = score::NumberedWithPath_Event<IntervalModel, MoveOnSlotHandle>;
// using ClickOnSlotHandle_Event
//     = score::NumberedWithPath_Event<IntervalModel, ClickOnSlotHandle>;
struct ClickOnSlotHandle_Event : public score::NumberedEvent<ClickOnSlotHandle>
{
  explicit ClickOnSlotHandle_Event(const SlotPath& p) : NumberedEvent<ClickOnSlotHandle>(), path(p)
  {
  }

  explicit ClickOnSlotHandle_Event(SlotPath&& p)
      : NumberedEvent<ClickOnSlotHandle>(), path(std::move(p))
  {
  }

  SlotPath path;
};

struct MoveOnSlotHandle_Event : public score::NumberedEvent<MoveOnSlotHandle>
{
  explicit MoveOnSlotHandle_Event(const SlotPath& p) : NumberedEvent<MoveOnSlotHandle>(), path(p)
  {
  }

  explicit MoveOnSlotHandle_Event(SlotPath&& p)
      : NumberedEvent<MoveOnSlotHandle>(), path(std::move(p))
  {
  }

  SlotPath path;
};

// using ReleaseOnSlotHandle_Event
//     = score::NumberedWithPath_Event<IntervalModel, ReleaseOnSlotHandle>;
struct ReleaseOnSlotHandle_Event : public score::NumberedEvent<ReleaseOnSlotHandle>
{
  explicit ReleaseOnSlotHandle_Event(const SlotPath& p)
      : NumberedEvent<ReleaseOnSlotHandle>(), path(p)
  {
  }

  explicit ReleaseOnSlotHandle_Event(SlotPath&& p)
      : NumberedEvent<ReleaseOnSlotHandle>(), path(std::move(p))
  {
  }

  SlotPath path;
};

template <int N>
QString debug_StateMachineIDs()
{
  QString txt;

  auto object = static_cast<ScenarioElement>(N % 10);
  auto modifier = static_cast<score::Modifier_tagme>((N - object) % 1000 / 100);
  switch (modifier)
  {
    case score::Modifier_tagme::Click:
      txt += "Click on";
      break;
    case score::Modifier_tagme::Move:
      txt += "Move on";
      break;
    case score::Modifier_tagme::Release:
      txt += "Release on";
      break;
  }

  switch (object)
  {
    case ScenarioElement::Nothing:
      txt += "nothing";
      break;
    case ScenarioElement::TimeSync:
      txt += "Sync";
      break;
    case ScenarioElement::Event:
      txt += "Event";
      break;
    case ScenarioElement::Interval:
      txt += "Interval";
      break;
    case ScenarioElement::State:
      txt += "State";
      break;
    case ScenarioElement::SlotOverlay_e:
      txt += "SlotOverlay_e";
      break;
    case ScenarioElement::SlotHandle_e:
      txt += "SlotHandle_e";
      break;
    case ScenarioElement::Trigger:
      txt += "Trigger";
      break;
    case ScenarioElement::LeftBrace:
      txt += "LeftBrace";
      break;
    case ScenarioElement::RightBrace:
      txt += "RightBrace";
      break;
  }

  return txt;
}
}
