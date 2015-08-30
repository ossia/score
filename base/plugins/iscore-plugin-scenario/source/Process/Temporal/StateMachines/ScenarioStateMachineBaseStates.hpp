#pragma once
#include "Process/Temporal/StateMachines/ScenarioPoint.hpp"

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

// OPTIMIZEME this when we have all the tools
class ScenarioStateBase : public QState
{
    public:
        ScenarioStateBase(const Path<ScenarioModel>& scenar, QState* parent):
            QState{parent},
            m_scenarioPath{scenar}
        { }

        void clear()
        {
            clickedEvent = Id<EventModel>{};
            clickedTimeNode = Id<TimeNodeModel>{};
            clickedConstraint = Id<ConstraintModel>{};
            clickedState = Id<StateModel>{};

            currentPoint = ScenarioPoint();
        }

        Id<StateModel> clickedState;
        Id<EventModel> clickedEvent;
        Id<TimeNodeModel> clickedTimeNode;
        Id<ConstraintModel> clickedConstraint;

        Id<StateModel> hoveredState;
        Id<EventModel> hoveredEvent;
        Id<TimeNodeModel> hoveredTimeNode;
        Id<ConstraintModel> hoveredConstraint;

        ScenarioPoint currentPoint;

    protected:
        Path<ScenarioModel> m_scenarioPath;
};

class CreationState : public ScenarioStateBase
{
    public:
        using ScenarioStateBase::ScenarioStateBase;

        QVector<Id<StateModel>> createdStates;
        QVector<Id<EventModel>> createdEvents;
        QVector<Id<TimeNodeModel>> createdTimeNodes;
        QVector<Id<ConstraintModel>> createdConstraints;

        void clearCreatedIds()
        {
            createdEvents.clear();
            createdConstraints.clear();
            createdTimeNodes.clear();
            createdStates.clear();
        }
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
