#pragma once
#include "Process/Temporal/StateMachines/ScenarioPoint.hpp"

#include <iscore/tools/ObjectPath.hpp>
#include <iscore/statemachine/StateMachineUtils.hpp>

#include <QStateMachine>
#include <QState>
#include <QAbstractTransition>
#include <QPointF>

class EventModel;
class TimeNodeModel;
class ConstraintModel;
class StateModel;

// TODO optimize this when we have all the tools
class ScenarioStateBase : public QState
{
    public:
        ScenarioStateBase(ObjectPath&& scenar, QState* parent):
            QState{parent},
            m_scenarioPath{std::move(scenar)}
        { }

        void clear()
        {
            clickedEvent = id_type<EventModel>{};
            clickedTimeNode = id_type<TimeNodeModel>{};
            clickedConstraint = id_type<ConstraintModel>{};
            clickedState = id_type<StateModel>{};

            currentPoint = ScenarioPoint();
        }

        id_type<StateModel> clickedState;
        id_type<EventModel> clickedEvent;
        id_type<TimeNodeModel> clickedTimeNode;
        id_type<ConstraintModel> clickedConstraint;

        id_type<StateModel> hoveredState;
        id_type<EventModel> hoveredEvent;
        id_type<TimeNodeModel> hoveredTimeNode;
        id_type<ConstraintModel> hoveredConstraint;

        ScenarioPoint currentPoint;

    protected:
        ObjectPath m_scenarioPath;
};

class CreationState : public ScenarioStateBase
{
    public:
        using ScenarioStateBase::ScenarioStateBase;

        QVector<id_type<StateModel>> createdStates;
        QVector<id_type<EventModel>> createdEvents;
        QVector<id_type<TimeNodeModel>> createdTimeNodes;
        QVector<id_type<ConstraintModel>> createdConstraints;

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

        ObjectPath currentSlot;

        QPointF m_originalPoint;
        double m_originalHeight{};
};
