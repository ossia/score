#pragma once
#include <iscore/tools/SettableIdentifier.hpp>
#include <ProcessInterface/TimeValue.hpp>

class ScenarioModel;
class EventModel;
class ConstraintModel;
class ConstraintViewModel;
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
                const Id<TimeNodeModel>& id,
                ScenarioModel& s);

        static TimeNodeModel& redo(
                const Id<TimeNodeModel>& id,
                const VerticalExtent& extent,
                const TimeValue& date,
                ScenarioModel& s);
};

template<>
class ScenarioCreate<EventModel>
{
    public:
        static void undo(
                const Id<EventModel>& id,
                ScenarioModel& s);

        static EventModel& redo(
                const Id<EventModel>& id,
                TimeNodeModel& timenode,
                const VerticalExtent& extent,
                ScenarioModel& s);
};

template<>
class ScenarioCreate<StateModel>
{
    public:
        static void undo(
                const Id<StateModel>& id,
                ScenarioModel& s);

        static StateModel& redo(
                const Id<StateModel>& id,
                EventModel& ev,
                double y,
                ScenarioModel& s);
};

template<>
class ScenarioCreate<ConstraintModel>
{
    public:
        static void undo(
                const Id<ConstraintModel>& id,
                ScenarioModel& s);

        static ConstraintModel& redo(
                const Id<ConstraintModel>& id,
                const Id<ConstraintViewModel>& fullviewid,
                StateModel& sst,
                StateModel& est,
                double ypos,
                ScenarioModel& s);
};
