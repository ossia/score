#pragma once
#include <iscore/tools/IdentifiedObject.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/plugins/documentdelegate/plugin/ElementPluginModelList.hpp>

#include "Process/ScenarioInterface.hpp"
class ConstraintModel;
class TimeNodeModel;
class EventModel;
class StateModel;
class DataStream;
class JSONObject;

class BaseScenario : public IdentifiedObject<BaseScenario>, public ScenarioInterface
{
        ISCORE_SERIALIZE_FRIENDS(BaseScenario, DataStream)
        ISCORE_SERIALIZE_FRIENDS(BaseScenario, JSONObject)

    public:
        iscore::ElementPluginModelList pluginModelList;
        BaseScenario(const Id<BaseScenario>&, QObject* parent);

        template<typename DeserializerVisitor,
                 enable_if_deserializer<DeserializerVisitor>* = nullptr>
        BaseScenario(DeserializerVisitor&& vis, QObject* parent) :
            IdentifiedObject{vis, parent}
        {
            vis.writeTo(*this);
        }

        ConstraintModel &constraint(const Id<ConstraintModel> &constraintId) const override;
        EventModel &event(const Id<EventModel> &eventId) const override;
        TimeNodeModel &timeNode(const Id<TimeNodeModel> &timeNodeId) const override;
        StateModel &state(const Id<StateModel> &stId) const override;

        ConstraintModel& baseConstraint() const;

        TimeNodeModel& startTimeNode() const;
        TimeNodeModel& endTimeNode() const;

        EventModel& startEvent() const;
        EventModel& endEvent() const;

        StateModel& startState() const;
        StateModel& endState() const;

        bool event(QEvent* ev) override { return QObject::event(ev); }
    private:
        TimeNodeModel* m_startNode{};
        TimeNodeModel* m_endNode{};

        EventModel* m_startEvent{};
        EventModel* m_endEvent{};

        StateModel* m_startState{};
        StateModel* m_endState{};

        ConstraintModel* m_constraint{};
};
