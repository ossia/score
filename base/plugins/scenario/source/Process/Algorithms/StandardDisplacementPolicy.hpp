#pragma once
#include <ProcessInterface/TimeValue.hpp>
#include <tools/SettableIdentifier.hpp>

class ScenarioModel;
class EventModel;
class ConstraintModel;
class TimeNodeModel;

namespace StandardDisplacementPolicy
{
    void setEventPosition (ScenarioModel& scenario,
                           id_type<EventModel> eventId,
                           TimeValue absolute_time,
                           double heightPosition);

    void setConstraintPosition (ScenarioModel& scenario,
                                id_type<ConstraintModel> constraintId,
                                TimeValue absolute_time,
                                double heightPosition);
}
