#pragma once
#include <Scenario/Process/ScenarioInterface.hpp>
#include <tuple>

#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/tools/std/IndirectContainer.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore_plugin_scenario_export.h>

class DataStream;

class JSONObject;
class QObject;

namespace Scenario
{
class ConstraintModel;
class EventModel;
class StateModel;
class TimeNodeModel;
class ISCORE_PLUGIN_SCENARIO_EXPORT BaseScenarioContainer :
        public ScenarioInterface
{
        ISCORE_SERIALIZE_FRIENDS(BaseScenarioContainer, DataStream)
        ISCORE_SERIALIZE_FRIENDS(BaseScenarioContainer, JSONObject)
    public:
        explicit BaseScenarioContainer(QObject* parentObject):
            m_parent{parentObject}
        {

        }

        virtual ~BaseScenarioContainer();

        QObject& parentObject() const { return *m_parent; }
        void init();
        void init(const BaseScenarioContainer&); // clone

        ConstraintModel* findConstraint(
                const Id<ConstraintModel>& id) const final override;

        EventModel* findEvent(
                const Id<EventModel>& id) const final override;

        TimeNodeModel* findTimeNode(
                const Id<TimeNodeModel>& id) const final override;

        StateModel* findState(
                const Id<StateModel>& id) const final override;

        ConstraintModel &constraint(
                const Id<ConstraintModel> &id) const final override;

        EventModel &event(
                const Id<EventModel> &id) const final override;

        TimeNodeModel &timeNode(
                const Id<TimeNodeModel> &id) const final override;

        StateModel &state(
                const Id<StateModel> &id) const final override;

        ConstraintModel& constraint() const;

        TimeNodeModel& startTimeNode() const final override;
        TimeNodeModel& endTimeNode() const;

        EventModel& startEvent() const;
        EventModel& endEvent() const;

        StateModel& startState() const;
        StateModel& endState() const;

        IndirectArray<ConstraintModel, 1> constraints() const
        {
            return m_constraint;
        }

        IndirectArray<EventModel, 2> events() const
        {
            return {m_startEvent, m_endEvent};
        }
        IndirectArray<StateModel, 2>  states() const
        {
            return {m_startState, m_endState};
        }
        IndirectArray<TimeNodeModel, 2>  timeNodes() const
        {
            return {m_startNode, m_endNode};
        }

    protected:
        TimeNodeModel* m_startNode{};
        TimeNodeModel* m_endNode{};

        EventModel* m_startEvent{};
        EventModel* m_endEvent{};

        StateModel* m_startState{};
        StateModel* m_endState{};

        ConstraintModel* m_constraint{};

        auto elements() const
        { return std::make_tuple(m_startNode, m_endNode, m_startEvent, m_endEvent, m_startState, m_endState, m_constraint); }

    private:
        QObject* m_parent{}; // Parent for the constraints, timenodes, etc.
        // If inheriting, m_parent should be this.
};
}

