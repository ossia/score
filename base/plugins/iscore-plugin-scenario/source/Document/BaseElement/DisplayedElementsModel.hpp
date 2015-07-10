#pragma once
#include <QObject>
#include <iscore/selection/Selection.hpp>

class ConstraintModel;
class EventModel;
class StateModel;
class DisplayedElementsModel
{
    public:
        void setSelection(const Selection&);

        // Will set all the other
        void setDisplayedConstraint(const ConstraintModel* cst);
        const ConstraintModel& displayedConstraint() const;

        const EventModel& startEvent() const;
        const EventModel& endEvent() const;

        const StateModel& startState() const;

        const StateModel& endState() const;

    private:
        const EventModel* m_startEvent{};
        const EventModel* m_endEvent{};

        const StateModel* m_startState{};
        const StateModel* m_endState{};

        const ConstraintModel* m_constraint{};
};
