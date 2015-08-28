#pragma once
#include <iscore/tools/SettableIdentifier.hpp>
class ConstraintModel;
class EventModel;
class TimeNodeModel;
class StateModel;

class ScenarioInterface
{
    public:
        virtual ConstraintModel& constraint(const Id<ConstraintModel>& constraintId) const = 0;
        virtual EventModel& event(const Id<EventModel>& eventId) const = 0;
        virtual TimeNodeModel& timeNode(const Id<TimeNodeModel>& timeNodeId) const = 0;
        virtual StateModel& state(const Id<StateModel>& stId) const = 0;
};
