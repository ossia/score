#pragma once
#include <iscore/tools/SettableIdentifier.hpp>
#include <ProcessInterface/TimeValue.hpp>

class ScenarioModel;
class EventModel;
class ConstraintModel;
class AbstractConstraintViewModel;
class TimeNodeModel;



class CreateTimeNodeMin
{
    public:
        static void undo(
                const id_type<TimeNodeModel>& id,
                ScenarioModel& s);

        static TimeNodeModel& redo(
                const id_type<TimeNodeModel>& id,
                const TimeValue& date,
                double y,
                ScenarioModel& s);
};

class CreateEventMin
{
    public:
        static void undo(
                const id_type<EventModel>& id,
                ScenarioModel& s);

        static EventModel& redo(
                const id_type<EventModel>& id,
                TimeNodeModel& timenode,
                double y,
                ScenarioModel& s);
};

class CreateConstraintMin
{
    public:
        static void undo(
                const id_type<ConstraintModel>& id,
                ScenarioModel& s);

        static ConstraintModel& redo(
                const id_type<ConstraintModel>& id,
                const id_type<AbstractConstraintViewModel>& fullviewid,
                EventModel& sev,
                EventModel& eev,
                double ypos,
                ScenarioModel& s);
};


inline EventModel& CreateTimenodeAndEvent(
        const id_type<EventModel>& ev_id,
        const id_type<TimeNodeModel>& tn_id,
        const TimeValue& date,
        double y,
        ScenarioModel& s)
{
    auto& tn = CreateTimeNodeMin::redo(tn_id, date, y, s);
    return CreateEventMin::redo(ev_id, tn, y, s);
}

inline void CreateConstraintAndEvent(
        const id_type<ConstraintModel>& id_cst,
        const id_type<AbstractConstraintViewModel>& id_fv,
        EventModel& sev,
        TimeNodeModel& tn,
        const id_type<EventModel>& ev_id,
        const TimeValue& date,
        double y,
        ScenarioModel& s)
{
    CreateConstraintMin::redo(id_cst, id_fv, sev, CreateEventMin::redo(ev_id, tn, y, s), y, s);
}



inline void CreateTimenodeConstraintAndEvent(
        const id_type<ConstraintModel>& id_cst,
        const id_type<AbstractConstraintViewModel>& id_fv,
        EventModel& sev,
        const id_type<EventModel>& ev_id,
        const id_type<TimeNodeModel>& tn_id,
        const TimeValue& date,
        double y,
        ScenarioModel& s)
{
    CreateConstraintMin::redo(id_cst, id_fv, sev, CreateTimenodeAndEvent(ev_id, tn_id, date, y, s), y, s);
}
