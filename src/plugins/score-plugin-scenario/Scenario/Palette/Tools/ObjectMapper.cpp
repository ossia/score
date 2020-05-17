#include "ObjectMapper.hpp"
#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <Scenario/Document/Event/EventView.hpp>
#include <Scenario/Document/Event/ConditionView.hpp>
#include <Scenario/Document/Interval/IntervalView.hpp>
#include <Scenario/Document/TimeSync/TimeSyncView.hpp>
#include <Scenario/Document/TimeSync/TriggerView.hpp>

#include <Scenario/Document/Interval/Temporal/Braces/LeftBrace.hpp>
#include <Scenario/Document/State/StateView.hpp>
#include <Scenario/Document/State/StateMenuOverlay.hpp>
#include <Scenario/Document/Interval/IntervalHeader.hpp>

#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Event/EventPresenter.hpp>
#include <Scenario/Document/Interval/Graph/GraphIntervalPresenter.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Interval/IntervalPresenter.hpp>
#include <Scenario/Document/Interval/Slot.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/State/StatePresenter.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncPresenter.hpp>

namespace Scenario
{

OptionalId<EventModel> ObjectMapper::itemToEventId(const QGraphicsItem* pressedItem, const QObject* parentModel) noexcept
{
  const auto& event
      = static_cast<const EventView*>(pressedItem)->presenter().model();
  return event.parent() == parentModel
      ? event.id()
      : OptionalId<EventModel>{};
}

OptionalId<EventModel> ObjectMapper::itemToConditionId(const QGraphicsItem* pressedItem, const QObject* parentModel) noexcept
{
  const auto& event
      = static_cast<const EventView*>(pressedItem->parentItem())
      ->presenter()
      .model();
  return event.parent() == parentModel
      ? event.id()
      : OptionalId<EventModel>{};
}

OptionalId<TimeSyncModel> ObjectMapper::itemToTimeSyncId(const QGraphicsItem* pressedItem, const QObject* parentModel) noexcept
{
  const auto& timesync
      = static_cast<const TimeSyncView*>(pressedItem)->presenter().model();
  return timesync.parent() == parentModel
      ? timesync.id()
      : OptionalId<TimeSyncModel>{};
}

OptionalId<TimeSyncModel> ObjectMapper::itemToTriggerId(const QGraphicsItem* pressedItem, const QObject* parentModel) noexcept
{
  const auto& timesync
      = static_cast<const TimeSyncView*>(pressedItem->parentItem())
      ->presenter()
      .model();
  return timesync.parent() == parentModel
      ? timesync.id()
      : OptionalId<TimeSyncModel>{};
}

OptionalId<IntervalModel> ObjectMapper::itemToIntervalId(const QGraphicsItem* pressedItem, const QObject* parentModel) noexcept
{
  const auto& interval
      = static_cast<const IntervalView*>(pressedItem)->presenter().model();
  return interval.parent() == parentModel
      ? interval.id()
      : OptionalId<IntervalModel>{};
}

OptionalId<IntervalModel> ObjectMapper::itemToGraphIntervalId(const QGraphicsItem* pressedItem, const QObject* parentModel) noexcept
{
  const auto& interval
      = static_cast<const GraphalIntervalPresenter*>(pressedItem)->model();
  return interval.parent() == parentModel
      ? interval.id()
      : OptionalId<IntervalModel>{};
}

OptionalId<StateModel> ObjectMapper::itemToStateId(const QGraphicsItem* pressedItem, const QObject* parentModel) noexcept
{
  const auto& state
      = static_cast<const StateView*>(pressedItem)->presenter().model();

  return state.parent() == parentModel
      ? state.id()
      : OptionalId<StateModel>{};
}

optional<SlotPath> ObjectMapper::itemToIntervalFromHeader(const QGraphicsItem* pressedItem, const QObject* parentModel) noexcept
{
  auto handle = static_cast<const SlotHeader*>(pressedItem);
  const auto& cst = handle->presenter().model();

  if (cst.parent() == parentModel)
  {
    auto fv = isInFullView(cst) ? Slot::FullView : Slot::SmallView;
    return SlotPath{cst, handle->slotIndex(), fv};
  }
  else
  {
    return ossia::none;
  }
}

optional<SlotPath> ObjectMapper::itemToIntervalFromFooter(const QGraphicsItem* pressedItem, const QObject* parentModel) noexcept
{
  auto handle = static_cast<const SlotFooter*>(pressedItem);
  const auto& cst = handle->presenter().model();

  if (cst.parent() == parentModel)
  {
    auto fv = isInFullView(cst) ? Slot::FullView : Slot::SmallView;
    return SlotPath{cst, handle->slotIndex(), fv};
  }
  else
  {
    return ossia::none;
  }
}

}
