#pragma once
#include <iscore/tools/SettableIdentifier.hpp>
#include <ProcessInterface/TimeValue.hpp>

class ScenarioModel;
class EventModel;
class ConstraintModel;
class AbstractConstraintViewModel;
class TimeNodeModel;
class DisplayedStateModel;


class CreateTimeNodeMin
{
    public:
        static void undo(
                const id_type<TimeNodeModel>& id,
                ScenarioModel& s);

        static TimeNodeModel& redo(
                const id_type<TimeNodeModel>& id,
                const TimeValue& date,
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
                ScenarioModel& s);
};

class CreateStateMin
{
    public:
        static void undo(
                const id_type<DisplayedStateModel>& id,
                ScenarioModel& s);

        static DisplayedStateModel& redo(const id_type<DisplayedStateModel>& id,
                EventModel& ev,
                double y,
                ScenarioModel& s);
};

class CreateConstraintMin
{
    public:
        static void undo(
                const id_type<ConstraintModel>& id,
                ScenarioModel& s);

        static ConstraintModel& redo(const id_type<ConstraintModel>& id,
                const id_type<AbstractConstraintViewModel>& fullviewid,
                DisplayedStateModel& sst,
                DisplayedStateModel& est,
                double ypos,
                ScenarioModel& s);
};


