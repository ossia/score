#pragma once
#include <QPointer>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
class QGraphicsItem;
namespace Scenario
{
class FullViewConstraintPresenter;
class StatePresenter;
class EventPresenter;
class TimeSyncPresenter;
class TimeSyncModel;
struct DisplayedElementsContainer
{

  DisplayedElementsContainer() = default;

  DisplayedElementsContainer(
      ConstraintModel& cst,
      const StateModel& sst,
      const StateModel& est,
      const EventModel& sev,
      const EventModel& eev,
      const TimeSyncModel& stn,
      const TimeSyncModel& etn)
      : constraint{&cst}
      , startState{&sst}
      , endState{&est}
      , startEvent{&sev}
      , endEvent{&eev}
      , startNode{&stn}
      , endNode{&etn}
  {
  }

  QPointer<ConstraintModel> constraint{};
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
      FullViewConstraintPresenter* cp,
      StatePresenter* s1,
      StatePresenter* s2,
      EventPresenter* e1,
      EventPresenter* e2,
      TimeSyncPresenter* t1,
      TimeSyncPresenter* t2)
      : constraint{cp}
      , startState{s1}
      , endState{s2}
      , startEvent{e1}
      , endEvent{e2}
      , startNode{t1}
      , endNode{t2}
  {
  }

  FullViewConstraintPresenter* constraint{};
  StatePresenter* startState{};
  StatePresenter* endState{};
  EventPresenter* startEvent{};
  EventPresenter* endEvent{};
  TimeSyncPresenter* startNode{};
  TimeSyncPresenter* endNode{};
};
}
