#pragma once
#include <iscore/tools/SettableIdentifier.hpp>
class ConstraintModel;
class EventModel;
class TimeNodeModel;
class StateModel;

class ScenarioInterface
{
    public:
        virtual ConstraintModel& constraint(const id_type<ConstraintModel>& constraintId) const = 0;
        virtual EventModel& event(const id_type<EventModel>& eventId) const = 0;
        virtual TimeNodeModel& timeNode(const id_type<TimeNodeModel>& timeNodeId) const = 0;
        virtual StateModel& state(const id_type<StateModel>& stId) const = 0;
};
