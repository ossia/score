#include "Document/Event/EventModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/TimeNode/TimeNodeModel.hpp"
#include "Process/Temporal/TemporalScenarioLayerModel.hpp"
#include "ScenarioModel.hpp"
#include <iscore/serialization/JSONValueVisitor.hpp>

template<>
void Visitor<Reader<DataStream>>::readFrom(const ScenarioModel& scenario)
{
    readFrom(*scenario.pluginModelList);

    m_stream << scenario.m_startTimeNodeId
             << scenario.m_endTimeNodeId;
    m_stream << scenario.m_startEventId
             << scenario.m_endEventId;

    // Timenodes
    const auto& timenodes = scenario.timeNodes;
    m_stream << (int) timenodes.size();

    for(const auto& timenode : timenodes)
    {
        readFrom(timenode);
    }

    // Events
    const auto& events = scenario.events;
    m_stream << (int) events.size();

    for(const auto& event : events)
    {
        readFrom(event);
    }

    // States
    const auto& states = scenario.states;
    m_stream << (int) states.size();

    for(const auto& state : states)
    {
        readFrom(state);
    }

    // Constraints
    const auto& constraints = scenario.constraints;
    m_stream << (int) constraints.size();

    for(const auto& constraint : constraints)
    {
        readFrom(constraint);
    }



    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(ScenarioModel& scenario)
{
    scenario.pluginModelList = new iscore::ElementPluginModelList{*this, &scenario};

    m_stream >> scenario.m_startTimeNodeId
             >> scenario.m_endTimeNodeId;
    m_stream >> scenario.m_startEventId
             >> scenario.m_endEventId;

    // Timenodes
    int timenode_count;
    m_stream >> timenode_count;

    for(; timenode_count -- > 0;)
    {
        auto tnmodel = new TimeNodeModel {*this, &scenario};
        scenario.timeNodes.add(tnmodel);
    }

    // Events
    int event_count;
    m_stream >> event_count;

    for(; event_count -- > 0;)
    {
        auto evmodel = new EventModel {*this, &scenario};
        scenario.events.add(evmodel);
    }

    // Events
    int state_count;
    m_stream >> state_count;

    for(; state_count -- > 0;)
    {
        auto stmodel = new StateModel {*this, &scenario};
        scenario.states.add(stmodel);
    }

    // Constraints
    int constraint_count;
    m_stream >> constraint_count;

    for(; constraint_count -- > 0;)
    {
        auto constraint = new ConstraintModel {*this, &scenario};
        scenario.constraints.add(constraint);
    }

    checkDelimiter();
}




template<>
void Visitor<Reader<JSONObject>>::readFrom(const ScenarioModel& scenario)
{
    m_obj["PluginsMetadata"] = toJsonValue(*scenario.pluginModelList);

    m_obj["StartTimeNodeId"] = toJsonValue(scenario.m_startTimeNodeId);
    m_obj["EndTimeNodeId"] = toJsonValue(scenario.m_endTimeNodeId);
    m_obj["StartEventId"] = toJsonValue(scenario.m_startEventId);
    m_obj["EndEventId"] = toJsonValue(scenario.m_endEventId);

    m_obj["TimeNodes"] = toJsonArray(scenario.timeNodes);
    m_obj["Events"] = toJsonArray(scenario.events);
    m_obj["States"] = toJsonArray(scenario.states);
    m_obj["Constraints"] = toJsonArray(scenario.constraints);
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(ScenarioModel& scenario)
{
    Deserializer<JSONValue> elementPluginDeserializer(m_obj["PluginsMetadata"]);
    scenario.pluginModelList = new iscore::ElementPluginModelList{elementPluginDeserializer, &scenario};

    scenario.m_startTimeNodeId = fromJsonValue<Id<TimeNodeModel>> (m_obj["StartTimeNodeId"]);
    scenario.m_endTimeNodeId = fromJsonValue<Id<TimeNodeModel>> (m_obj["EndTimeNodeId"]);
    scenario.m_startEventId = fromJsonValue<Id<EventModel>> (m_obj["StartEventId"]);
    scenario.m_endEventId = fromJsonValue<Id<EventModel>> (m_obj["EndEventId"]);

    for(const auto& json_vref : m_obj["TimeNodes"].toArray())
    {
        auto tnmodel = new TimeNodeModel {
                       Deserializer<JSONObject>{json_vref.toObject() },
                       &scenario};

        scenario.timeNodes.add(tnmodel);
    }

    for(const auto& json_vref : m_obj["Events"].toArray())
    {
        auto evmodel = new EventModel {
                       Deserializer<JSONObject>{json_vref.toObject() },
                       &scenario};

        scenario.events.add(evmodel);
    }

    for(const auto& json_vref : m_obj["States"].toArray())
    {
        auto stmodel = new StateModel {
                       Deserializer<JSONObject>{json_vref.toObject() },
                       &scenario};

        scenario.states.add(stmodel);
    }

    for(const auto& json_vref : m_obj["Constraints"].toArray())
    {
        auto constraint = new ConstraintModel{
                Deserializer<JSONObject>{json_vref.toObject() },
                &scenario};
        scenario.constraints.add(constraint);
    }
}


#include <iscore/serialization/VisitorCommon.hpp>
void ScenarioModel::serialize(const VisitorVariant& vis) const
{
    serialize_dyn(vis, *this);
}

#include "ScenarioFactory.hpp"
Process* ScenarioFactory::loadModel(
        const VisitorVariant& vis,
        QObject* parent)
{
    return deserialize_dyn(vis, [&] (auto&& deserializer)
    { return new ScenarioModel{deserializer, parent};});
}

LayerModel* ScenarioModel::loadLayer_impl(
        const VisitorVariant& vis,
        QObject* parent)
{
    return deserialize_dyn(vis, [&] (auto&& deserializer)
    {
        auto scen = new TemporalScenarioLayerModel{
                            deserializer, *this, parent};
        this->makeLayer_impl(scen);
        return scen;
    });
}
