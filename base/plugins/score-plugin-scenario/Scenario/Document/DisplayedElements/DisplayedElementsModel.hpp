#pragma once
#include <Scenario/Document/DisplayedElements/DisplayedElementsContainer.hpp>

#include <score/selection/Selection.hpp>

#include <tuple>

namespace Scenario
{
class IntervalModel;
class EventModel;
class StateModel;
class TimeSyncModel;

class SCORE_PLUGIN_SCENARIO_EXPORT DisplayedElementsModel
{
public:
  DisplayedElementsModel() = default;
  bool initialized() const
  {
    return m_initialized;
  }

  void setSelection(const Selection&);

  void setDisplayedElements(DisplayedElementsContainer&&);
  IntervalModel& interval() const;

  const TimeSyncModel& startTimeSync() const;
  const TimeSyncModel& endTimeSync() const;

  const EventModel& startEvent() const;
  const EventModel& endEvent() const;

  const StateModel& startState() const;
  const StateModel& endState() const;

private:
  auto elements() const
  {
    return std::make_tuple(
        m_elements.startNode, m_elements.endNode, m_elements.startEvent,
        m_elements.endEvent, m_elements.startState, m_elements.endState,
        m_elements.interval);
  }

  DisplayedElementsContainer m_elements;
  bool m_initialized = false;
};
}
