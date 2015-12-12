#pragma once
#include <Scenario/Document/DisplayedElements/DisplayedElementsContainer.hpp>
#include <iscore/selection/Selection.hpp>
#include <tuple>

class ConstraintModel;
class EventModel;
class StateModel;
class TimeNodeModel;

class DisplayedElementsModel
{
    public:
        DisplayedElementsModel() = default;
        bool initialized() const { return m_initialized; }

        void setSelection(const Selection&);

        void setDisplayedElements(DisplayedElementsContainer&&);
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
                        m_elements.startNode,
                        m_elements.endNode,
                        m_elements.startEvent,
                        m_elements.endEvent,
                        m_elements.startState,
                        m_elements.endState,
                        m_elements.constraint);
        }

        DisplayedElementsContainer m_elements;
        bool m_initialized = false;
};
