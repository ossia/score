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
  bool initialized() const { return m_initialized; }
  //! False once the displayed interval is gone, e.g. while its document closes.
  bool hasInterval() const { return m_initialized && m_elements.interval; }

  void setSelection(const Selection&);

  void setDisplayedElements(DisplayedElementsContainer&&);
  IntervalModel& interval() const;

  TimeSyncModel& startTimeSync() const;
  TimeSyncModel& endTimeSync() const;

  EventModel& startEvent() const;
  EventModel& endEvent() const;

  StateModel& startState() const;
  StateModel& endState() const;

private:
  DisplayedElementsContainer m_elements;
  bool m_initialized = false;
};
}
