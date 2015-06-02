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

            currentPoint = ScenarioPoint();
        }

        id_type<EventModel> clickedEvent;
        id_type<TimeNodeModel> clickedTimeNode;
        id_type<ConstraintModel> clickedConstraint;

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
        const auto& createdEvent() const
        { return m_createdEvent; }
        void setCreatedEvent(const id_type<EventModel>& id)
        { m_createdEvent = id; }

        const auto& createdTimeNode() const
        { return m_createdTimeNode; }
        void setCreatedTimeNode(const id_type<TimeNodeModel>& id)
        { m_createdTimeNode = id; }

        const auto& createdConstraint() const
        { return m_createdConstraint; }
        void setCreatedConstraint(const id_type<ConstraintModel>& id)
        { m_createdConstraint = id; }

    private:
        id_type<EventModel> m_createdEvent;
        id_type<TimeNodeModel> m_createdTimeNode;
        id_type<ConstraintModel> m_createdConstraint;
};

class DeckState : public QState
{
    public:
        DeckState(QState* parent):
            QState{parent}
        { }

        ObjectPath currentDeck;

        QPointF m_originalPoint;
        double m_originalHeight{};
};
