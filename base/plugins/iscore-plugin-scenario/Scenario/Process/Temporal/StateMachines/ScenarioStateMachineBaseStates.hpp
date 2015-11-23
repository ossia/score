#pragma once
#include <Scenario/Process/Temporal/StateMachines/ScenarioPoint.hpp>

#include <iscore/tools/ModelPath.hpp>
#include <iscore/statemachine/StateMachineUtils.hpp>

#include <QStateMachine>
#include <QState>
#include <QAbstractTransition>
#include <QPointF>

class ScenarioModel;
class EventModel;
class TimeNodeModel;
class ConstraintModel;
class StateModel;
class SlotModel;

namespace Scenario
{
// OPTIMIZEME this when we have all the tools
template<typename Scenario_T>
class StateBase : public QState
{
    public:
        StateBase(const Path<ScenarioModel>& scenar, QState* parent):
            QState{parent},
            m_scenarioPath{scenar}
        { }

        void clear()
        {
            clickedEvent = Id<EventModel>{};
            clickedTimeNode = Id<TimeNodeModel>{};
            clickedConstraint = Id<ConstraintModel>{};
            clickedState = Id<StateModel>{};

            currentPoint = Scenario::Point();
        }

        Id<StateModel> clickedState;
        Id<EventModel> clickedEvent;
        Id<TimeNodeModel> clickedTimeNode;
        Id<ConstraintModel> clickedConstraint;

        Id<StateModel> hoveredState;
        Id<EventModel> hoveredEvent;
        Id<TimeNodeModel> hoveredTimeNode;
        Id<ConstraintModel> hoveredConstraint;

        Scenario::Point currentPoint;

    protected:
        Path<Scenario_T> m_scenarioPath;
};

class SlotState : public QState
{
    public:
        SlotState(QState* parent):
            QState{parent}
        { }

        Path<SlotModel> currentSlot;

        QPointF m_originalPoint;
        double m_originalHeight{};
};
}
