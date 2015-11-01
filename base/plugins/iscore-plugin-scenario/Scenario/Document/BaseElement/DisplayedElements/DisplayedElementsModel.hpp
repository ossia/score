#pragma once
#include <QObject>
#include <QPointer>
#include <iscore/selection/Selection.hpp>

class ConstraintModel;
class TimeNodeModel;
class EventModel;
class StateModel;
class DisplayedElementsModel
{
    public:
        void setSelection(const Selection&);

        // Will set all the other
        void setDisplayedConstraint(const ConstraintModel& cst);
        const ConstraintModel& displayedConstraint() const;

        const TimeNodeModel& startNode() const;
        const TimeNodeModel& endNode() const;

        const EventModel& startEvent() const;
        const EventModel& endEvent() const;

        const StateModel& startState() const;
        const StateModel& endState() const;

    private:
        QPointer<const TimeNodeModel> m_startNode{};
        QPointer<const TimeNodeModel> m_endNode{};

        QPointer<const EventModel> m_startEvent{};
        QPointer<const EventModel> m_endEvent{};

        QPointer<const StateModel> m_startState{};
        QPointer<const StateModel> m_endState{};

        QPointer<const ConstraintModel> m_constraint{};
};
