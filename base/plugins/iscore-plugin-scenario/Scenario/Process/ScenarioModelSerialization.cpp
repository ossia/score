#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioLayerModel.hpp>
#include "ScenarioFactory.hpp"

template<>
void Visitor<Reader<DataStream>>::readFrom(const ScenarioModel& scenario)
{
    readFrom(*scenario.pluginModelList);

    m_stream << scenario.metadata.name();

    m_stream << scenario.m_startTimeNodeId
             << scenario.m_endTimeNodeId;
    m_stream << scenario.m_startEventId
             << scenario.m_endEventId;

    // Constraints
    const auto& constraints = scenario.constraints;
    m_stream << (int32_t) constraints.size();

    for(const auto& constraint : constraints)
    {
        readFrom(constraint);
    }

    // Timenodes
    const auto& timenodes = scenario.timeNodes;
    m_stream << (int32_t) timenodes.size();

    for(const auto& timenode : timenodes)
    {
        readFrom(timenode);
    }

    // Events
    const auto& events = scenario.events;
    m_stream << (int32_t) events.size();

    for(const auto& event : events)
    {
        readFrom(event);
    }

    // States
    const auto& states = scenario.states;
    m_stream << (int32_t) states.size();

    for(const auto& state : states)
    {
        readFrom(state);
    }

    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(ScenarioModel& scenario)
{
    scenario.pluginModelList = new iscore::ElementPluginModelList{*this, &scenario};

    QString name;
    m_stream >> name;
    scenario.metadata.setName(name);
    m_stream >> scenario.m_startTimeNodeId
             >> scenario.m_endTimeNodeId;
    m_stream >> scenario.m_startEventId
             >> scenario.m_endEventId;

    // Constraints
    int32_t constraint_count;
    m_stream >> constraint_count;

    for(; constraint_count -- > 0;)
    {
        auto constraint = new ConstraintModel {*this, &scenario};
        scenario.constraints.add(constraint);
    }

    // Timenodes
    int32_t timenode_count;
    m_stream >> timenode_count;

    for(; timenode_count -- > 0;)
    {
        auto tnmodel = new TimeNodeModel {*this, &scenario};
        scenario.timeNodes.add(tnmodel);
    }

    // Events
    int32_t event_count;
    m_stream >> event_count;

    for(; event_count -- > 0;)
    {
        auto evmodel = new EventModel {*this, &scenario};
        scenario.events.add(evmodel);
    }

    // States
    int32_t state_count;
    m_stream >> state_count;

    for(; state_count -- > 0;)
    {
        auto stmodel = new StateModel {*this, &scenario};
        scenario.states.add(stmodel);
    }

    checkDelimiter();
}




template<>
void Visitor<Reader<JSONObject>>::readFrom(const ScenarioModel& scenario)
{
    m_obj["PluginsMetadata"] = toJsonValue(*scenario.pluginModelList);
    m_obj["Metadata"] = toJsonObject(scenario.metadata);

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
    scenario.metadata = fromJsonObject<ModelMetadata>(m_obj["Metadata"].toObject());

    scenario.m_startTimeNodeId = fromJsonValue<Id<TimeNodeModel>> (m_obj["StartTimeNodeId"]);
    scenario.m_endTimeNodeId = fromJsonValue<Id<TimeNodeModel>> (m_obj["EndTimeNodeId"]);
    scenario.m_startEventId = fromJsonValue<Id<EventModel>> (m_obj["StartEventId"]);
    scenario.m_endEventId = fromJsonValue<Id<EventModel>> (m_obj["EndEventId"]);

    for(const auto& json_vref : m_obj["Constraints"].toArray())
    {
        auto constraint = new ConstraintModel{
                Deserializer<JSONObject>{json_vref.toObject() },
                &scenario};
        scenario.constraints.add(constraint);
    }

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

}


void ScenarioModel::serialize(const VisitorVariant& vis) const
{
    serialize_dyn(vis, *this);
}

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
