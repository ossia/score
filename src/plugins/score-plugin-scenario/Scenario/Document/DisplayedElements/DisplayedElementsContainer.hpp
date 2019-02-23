#pragma once
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>

#include <QPointer>
class QGraphicsItem;
namespace Scenario
{
class FullViewIntervalPresenter;
class StatePresenter;
class EventPresenter;
class TimeSyncPresenter;
class TimeSyncModel;
struct DisplayedElementsContainer
{

  DisplayedElementsContainer() = default;

  DisplayedElementsContainer(
      IntervalModel& cst,
      const StateModel& sst,
      const StateModel& est,
      const EventModel& sev,
      const EventModel& eev,
      const TimeSyncModel& stn,
      const TimeSyncModel& etn)
      : interval{&cst}
      , startState{&sst}
      , endState{&est}
      , startEvent{&sev}
      , endEvent{&eev}
      , startNode{&stn}
      , endNode{&etn}
  {
  }

  QPointer<IntervalModel> interval{};
  QPointer<const StateModel> startState{};
  QPointer<const StateModel> endState{};
  QPointer<const EventModel> startEvent{};
  QPointer<const EventModel> endEvent{};
  QPointer<const TimeSyncModel> startNode{};
  QPointer<const TimeSyncModel> endNode{};
};

struct DisplayedElementsPresenterContainer
{

  DisplayedElementsPresenterContainer() = default;

  DisplayedElementsPresenterContainer(
      FullViewIntervalPresenter* cp,
      StatePresenter* s1,
      StatePresenter* s2,
      EventPresenter* e1,
      EventPresenter* e2,
      TimeSyncPresenter* t1,
      TimeSyncPresenter* t2)
      : interval{cp}
      , startState{s1}
      , endState{s2}
      , startEvent{e1}
      , endEvent{e2}
      , startNode{t1}
      , endNode{t2}
  {
  }

  FullViewIntervalPresenter* interval{};
  StatePresenter* startState{};
  StatePresenter* endState{};
  EventPresenter* startEvent{};
  EventPresenter* endEvent{};
  TimeSyncPresenter* startNode{};
  TimeSyncPresenter* endNode{};
};
}
