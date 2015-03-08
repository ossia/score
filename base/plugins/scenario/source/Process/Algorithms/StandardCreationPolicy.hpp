#pragma once
#include <iscore/tools/SettableIdentifier.hpp>
#include <ProcessInterface/TimeValue.hpp>

class ScenarioModel;
class EventModel;
class ConstraintModel;
class AbstractConstraintViewModel;
class TimeNodeModel;

namespace StandardCreationPolicy
{
    void createConstraintBetweenEvents(ScenarioModel& scenario,
                                       id_type<EventModel> startEventId,
                                       id_type<EventModel> endEventId,
                                       id_type<ConstraintModel> newConstraintModelId,
                                       id_type<AbstractConstraintViewModel> newConstraintFullViewId);


    void createConstraintAndEndEventFromEvent(ScenarioModel& scenario,
                                              id_type<EventModel> startEventId,
                                              TimeValue constraint_duration,
                                              double heightPos,
                                              id_type<ConstraintModel> newConstraintId,
                                              id_type<AbstractConstraintViewModel> newConstraintFullViewId,
                                              id_type<EventModel> newEventId);


    void createTimeNode(ScenarioModel& scenario,
                        id_type<TimeNodeModel> timeNodeId,
                        id_type<EventModel> eventId);
}
