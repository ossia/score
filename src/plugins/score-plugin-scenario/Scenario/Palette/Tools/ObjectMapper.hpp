#pragma once
#include <Scenario/Document/Interval/Slot.hpp>

#include <score/model/Identifier.hpp>

class QGraphicsItem;
namespace Scenario
{
class EventModel;
class TimeSyncModel;
class IntervalModel;
class StateModel;

struct SCORE_PLUGIN_SCENARIO_EXPORT ObjectMapper
{

  static OptionalId<EventModel>
  itemToEventId(const QGraphicsItem* pressedItem, const QObject* parentModel) noexcept;
  static OptionalId<EventModel>
  itemToConditionId(const QGraphicsItem* pressedItem, const QObject* parentModel) noexcept;
  static OptionalId<TimeSyncModel>
  itemToTimeSyncId(const QGraphicsItem* pressedItem, const QObject* parentModel) noexcept;
  static OptionalId<TimeSyncModel>
  itemToTriggerId(const QGraphicsItem* pressedItem, const QObject* parentModel) noexcept;
  static OptionalId<IntervalModel>
  itemToIntervalId(const QGraphicsItem* pressedItem, const QObject* parentModel) noexcept;
  static OptionalId<IntervalModel>
  itemToGraphIntervalId(const QGraphicsItem* pressedItem, const QObject* parentModel) noexcept;
  static OptionalId<StateModel>
  itemToStateId(const QGraphicsItem* pressedItem, const QObject* parentModel) noexcept;

  static std::optional<SlotPath>
  itemToIntervalFromHeader(const QGraphicsItem* pressedItem, const QObject* parentModel) noexcept;
  static std::optional<SlotPath>
  itemToIntervalFromFooter(const QGraphicsItem* pressedItem, const QObject* parentModel) noexcept;
};

}
