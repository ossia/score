#pragma once
#include <score/model/Identifier.hpp>

namespace Scenario
{
class IntervalModel;
class EventModel;
class EventPresenter;
class StatePresenter;
class TemporalIntervalPresenter;
class ScenarioPresenter;
class TimeSyncPresenter;
class CommentBlockPresenter;
class ScenarioViewInterface
{
public:
  ScenarioViewInterface(const ScenarioPresenter& presenter);

  void on_eventMoved(const EventPresenter& event);
  void on_intervalMoved(const TemporalIntervalPresenter& interval);
  void on_timeSyncMoved(const TimeSyncPresenter& timesync);
  void on_stateMoved(const StatePresenter& state);
  void on_commentMoved(const CommentBlockPresenter& comment);

private:
  const ScenarioPresenter& m_presenter;
};
}
