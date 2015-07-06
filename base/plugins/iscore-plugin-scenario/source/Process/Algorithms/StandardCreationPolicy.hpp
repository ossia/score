#pragma once
#include <iscore/tools/SettableIdentifier.hpp>
#include <ProcessInterface/TimeValue.hpp>

class ScenarioModel;
class EventModel;
class ConstraintModel;
class AbstractConstraintViewModel;
class TimeNodeModel;
class StateModel;
struct VerticalExtent;

template<typename T>
class ScenarioCreate;

template<>
class ScenarioCreate<TimeNodeModel>
{
    public:
        static void undo(
                const id_type<TimeNodeModel>& id,
                ScenarioModel& s);

        static TimeNodeModel& redo(
                const id_type<TimeNodeModel>& id,
                const VerticalExtent& extent,
                const TimeValue& date,
                ScenarioModel& s);
};

template<>
class ScenarioCreate<EventModel>
{
    public:
        static void undo(
                const id_type<EventModel>& id,
                ScenarioModel& s);

        static EventModel& redo(
                const id_type<EventModel>& id,
                TimeNodeModel& timenode,
                const VerticalExtent& extent,
                ScenarioModel& s);
};

template<>
class ScenarioCreate<StateModel>
{
    public:
        static void undo(
                const id_type<StateModel>& id,
                ScenarioModel& s);

        static StateModel& redo(
                const id_type<StateModel>& id,
                EventModel& ev,
                double y,
                ScenarioModel& s);
};

template<>
class ScenarioCreate<ConstraintModel>
{
    public:
        static void undo(
                const id_type<ConstraintModel>& id,
                ScenarioModel& s);

        static ConstraintModel& redo(
                const id_type<ConstraintModel>& id,
                const id_type<AbstractConstraintViewModel>& fullviewid,
                StateModel& sst,
                StateModel& est,
                double ypos,
                ScenarioModel& s);
};
