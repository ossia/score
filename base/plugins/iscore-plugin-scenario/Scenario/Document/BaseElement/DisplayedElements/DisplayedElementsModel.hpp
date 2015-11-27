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
        const ConstraintModel& constraint() const;

        const TimeNodeModel& startTimeNode() const;
        const TimeNodeModel& endTimeNode() const;

        const EventModel& startEvent() const;
        const EventModel& endEvent() const;

        const StateModel& startState() const;
        const StateModel& endState() const;

    private:
        auto elements() const
        {
            return std::make_tuple(
                        m_startNode,
                        m_endNode,
                        m_startEvent,
                        m_endEvent,
                        m_startState,
                        m_endState,
                        m_constraint);
        }

        QPointer<const TimeNodeModel> m_startNode{};
        QPointer<const TimeNodeModel> m_endNode{};

        QPointer<const EventModel> m_startEvent{};
        QPointer<const EventModel> m_endEvent{};

        QPointer<const StateModel> m_startState{};
        QPointer<const StateModel> m_endState{};

        QPointer<const ConstraintModel> m_constraint{};
};
