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

    TimeNodeModel* createTimeNode(ScenarioModel& scenario,
                             id_type<TimeNodeModel> timeNodeId,
                             id_type<EventModel> eventId);
}




class CreateTimeNodeMin
{
    public:
        static void undo(
                id_type<TimeNodeModel> id,
                ScenarioModel& s);

        static TimeNodeModel& redo(
                id_type<TimeNodeModel> id,
                const TimeValue& date,
                double y,
                ScenarioModel& s);
};

class CreateEventMin
{
    public:
        static void undo(id_type<EventModel> id,
                         ScenarioModel& s);

        static EventModel& redo(
                id_type<EventModel> id,
                TimeNodeModel& timenode,
                double y,
                ScenarioModel& s);
};

class CreateConstraintMin
{
    public:
        static void undo(
                id_type<ConstraintModel> id,
                ScenarioModel& s);

        static ConstraintModel& redo(id_type<ConstraintModel> id,
                                     id_type<AbstractConstraintViewModel> fullviewid,
                                     EventModel& sev,
                                     EventModel& eev,
                                     double ypos,
                                     ScenarioModel& s);
};


inline EventModel& CreateTimenodeAndEvent(id_type<EventModel> ev_id,
                                   id_type<TimeNodeModel> tn_id,
                                   const TimeValue& date,
                                   double y,
                                   ScenarioModel& s)
{
    auto& tn = CreateTimeNodeMin::redo(tn_id, date, y, s);
    return CreateEventMin::redo(ev_id, tn, y, s);
}

inline void CreateConstraintAndEvent(id_type<ConstraintModel> id_cst,
                                     id_type<AbstractConstraintViewModel> id_fv,
                                     EventModel& sev,
                                     TimeNodeModel& tn,
                                     id_type<EventModel> ev_id,
                                     const TimeValue& date,
                                     double y,
                                     ScenarioModel& s)
{
    CreateConstraintMin::redo(id_cst, id_fv, sev, CreateEventMin::redo(ev_id, tn, y, s), y, s);
}



inline void CreateTimenodeConstraintAndEvent(id_type<ConstraintModel> id_cst,
                                      id_type<AbstractConstraintViewModel> id_fv,
                                      EventModel& sev,
                                      id_type<EventModel> ev_id,
                                      id_type<TimeNodeModel> tn_id,
                                      const TimeValue& date,
                                      double y,
                                      ScenarioModel& s)
{
    CreateConstraintMin::redo(id_cst, id_fv, sev, CreateTimenodeAndEvent(ev_id, tn_id, date, y, s), y, s);
}
