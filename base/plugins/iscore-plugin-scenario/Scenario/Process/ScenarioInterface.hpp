#pragma once
class ConstraintModel;
class EventModel;
class StateModel;
class TimeNodeModel;
#include <iscore/tools/SettableIdentifier.hpp>

class ScenarioInterface
{
    public:
        virtual ~ScenarioInterface();
        virtual ConstraintModel* findConstraint(const Id<ConstraintModel>& constraintId) const = 0;
        virtual EventModel* findEvent(const Id<EventModel>& eventId) const = 0;
        virtual TimeNodeModel* findTimeNode(const Id<TimeNodeModel>& timeNodeId) const = 0;
        virtual StateModel* findState(const Id<StateModel>& stId) const = 0;

        virtual ConstraintModel& constraint(const Id<ConstraintModel>& constraintId) const = 0;
        virtual EventModel& event(const Id<EventModel>& eventId) const = 0;
        virtual TimeNodeModel& timeNode(const Id<TimeNodeModel>& timeNodeId) const = 0;
        virtual StateModel& state(const Id<StateModel>& stId) const = 0;

        virtual TimeNodeModel& startTimeNode() const = 0;
        virtual TimeNodeModel& endTimeNode() const = 0;
};


