#pragma once
#include <Scenario/Palette/ScenarioPoint.hpp>

#include <iscore/statemachine/StateMachineUtils.hpp>
#include <iscore/model/path/Path.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotModel.hpp>

#include <QAbstractTransition>
#include <QPointF>
#include <QState>
#include <QStateMachine>

namespace Scenario
{
class EventModel;
class TimeNodeModel;
class ConstraintModel;
class StateModel;
// OPTIMIZEME this when we have all the tools
template <typename Scenario_T>
class StateBase : public QState
{
public:
  StateBase(const Path<Scenario_T>& scenar, QState* parent)
      : QState{parent}, m_scenarioPath{scenar}
  {
  }

  void clear()
  {
    clickedEvent = ossia::none;
    clickedTimeNode = ossia::none;
    clickedConstraint = ossia::none;
    clickedState = ossia::none;

    hoveredEvent = ossia::none;
    hoveredTimeNode = ossia::none;
    hoveredConstraint = ossia::none;
    hoveredState = ossia::none;

    currentPoint = Scenario::Point();
  }

  OptionalId<StateModel> clickedState;
  OptionalId<EventModel> clickedEvent;
  OptionalId<TimeNodeModel> clickedTimeNode;
  OptionalId<ConstraintModel> clickedConstraint;

  OptionalId<StateModel> hoveredState;
  OptionalId<EventModel> hoveredEvent;
  OptionalId<TimeNodeModel> hoveredTimeNode;
  OptionalId<ConstraintModel> hoveredConstraint;

  Scenario::Point currentPoint;

protected:
  Path<Scenario_T> m_scenarioPath;
};

class SlotState : public QState
{
public:
  SlotState(QState* parent) : QState{parent}
  {
  }

  SlotPath currentSlot;

  QPointF m_originalPoint;
  double m_originalHeight{};
};
}
