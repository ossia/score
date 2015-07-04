#pragma once
#include <iscore/tools/IdentifiedObject.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/plugins/documentdelegate/plugin/ElementPluginModelList.hpp>
class ConstraintModel;
class TimeNodeModel;
class EventModel;
class DisplayedStateModel;
class DataStream;
class JSONObject;

class BaseScenario : public IdentifiedObject<BaseScenario>
{
        friend void Visitor<Reader<DataStream>>::readFrom<BaseScenario> (const BaseScenario&);
        friend void Visitor<Writer<DataStream>>::writeTo<BaseScenario> (BaseScenario&);
        friend void Visitor<Reader<JSONObject>>::readFrom<BaseScenario> (const BaseScenario&);
        friend void Visitor<Writer<JSONObject>>::writeTo<BaseScenario> (BaseScenario&);

    public:
        iscore::ElementPluginModelList pluginModelList;
        BaseScenario(const id_type<BaseScenario>&, QObject* parent);

        template<typename DeserializerVisitor,
                 enable_if_deserializer<DeserializerVisitor>* = nullptr>
        BaseScenario(DeserializerVisitor&& vis, QObject* parent) :
            IdentifiedObject<BaseScenario> {vis, parent}
        {
            vis.writeTo(*this);
        }


        ConstraintModel* baseConstraint() const;
        TimeNodeModel* startTimeNode() const;
        TimeNodeModel* endTimeNode() const;

        EventModel* startEvent() const;
        EventModel* endEvent() const;

    private:
        TimeNodeModel* m_startNode{};
        TimeNodeModel* m_endNode{};

        EventModel* m_startEvent{};
        EventModel* m_endEvent{};

        DisplayedStateModel* m_startState{};
        DisplayedStateModel* m_endState{};

        ConstraintModel* m_constraint{};

};
