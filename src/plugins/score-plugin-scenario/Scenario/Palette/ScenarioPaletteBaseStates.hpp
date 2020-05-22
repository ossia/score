#pragma once
#include <Scenario/Document/Interval/Slot.hpp>
#include <Scenario/Palette/ScenarioPoint.hpp>

#include <score/model/path/Path.hpp>
#include <score/statemachine/StateMachineUtils.hpp>

#include <QPointF>
#include <QState>

namespace Scenario
{
class EventModel;
class TimeSyncModel;
class IntervalModel;
class StateModel;
// OPTIMIZEME this when we have all the tools
template <typename Scenario_T>
class StateBase : public QState
{
public:
  StateBase(const Scenario_T& scenar, QState* parent)
      : QState{parent}, m_scenario{const_cast<Scenario_T&>(scenar)}
  {
  }

  void clear()
  {
    clickedEvent = std::nullopt;
    clickedTimeSync = std::nullopt;
    clickedInterval = std::nullopt;
    clickedState = std::nullopt;

    hoveredEvent = std::nullopt;
    hoveredTimeSync = std::nullopt;
    hoveredInterval = std::nullopt;
    hoveredState = std::nullopt;

    currentPoint = Scenario::Point();
  }

  OptionalId<StateModel> clickedState;
  OptionalId<EventModel> clickedEvent;
  OptionalId<TimeSyncModel> clickedTimeSync;
  OptionalId<IntervalModel> clickedInterval;

  OptionalId<StateModel> hoveredState;
  OptionalId<EventModel> hoveredEvent;
  OptionalId<TimeSyncModel> hoveredTimeSync;
  OptionalId<IntervalModel> hoveredInterval;

  Scenario::Point currentPoint{};

protected:
  Scenario_T& m_scenario;
};

class SCORE_PLUGIN_SCENARIO_EXPORT SlotState : public QState
{
public:
  SlotState(QState* parent) : QState{parent} { }
  ~SlotState() override;

  SlotPath currentSlot;

  QPointF m_originalPoint;
  double m_originalHeight{};
};
}
