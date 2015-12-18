#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioLayerModel.hpp>

#include <boost/optional/optional.hpp>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <sys/types.h>
#include <algorithm>

#include <Process/ModelMetadata.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include "ScenarioFactory.hpp"
#include <iscore/plugins/documentdelegate/plugin/ElementPluginModelList.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/serialization/VisitorCommon.hpp>
#include <iscore/tools/NotifyingMap.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/document/DocumentContext.hpp>
#include <iscore_plugin_scenario_export.h>
class LayerModel;
class Process;
class QObject;
struct VisitorVariant;
template <typename T> class Reader;
template <typename T> class Writer;

template<>
void Visitor<Reader<DataStream>>::readFrom(const Scenario::ScenarioModel& scenario)
{
    readFrom(*scenario.pluginModelList);

    m_stream << scenario.metadata.name();

    m_stream << scenario.m_startTimeNodeId
             << scenario.m_endTimeNodeId;
    m_stream << scenario.m_startEventId
             << scenario.m_endEventId;
    m_stream << scenario.m_startStateId;

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

    // Comments
    const auto& comments = scenario.comments;
    m_stream << (int32_t) comments.size();

    for(const auto& cmt : comments)
    {
        readFrom(cmt);
    }

    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(Scenario::ScenarioModel& scenario)
{
    scenario.pluginModelList = new iscore::ElementPluginModelList{*this, &scenario};

    QString name;
    m_stream >> name;
    scenario.metadata.setName(name);
    m_stream >> scenario.m_startTimeNodeId
             >> scenario.m_endTimeNodeId;
    m_stream >> scenario.m_startEventId
             >> scenario.m_endEventId;
    m_stream >> scenario.m_startStateId;

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

    auto& stack = iscore::IDocument::documentContext(scenario).commandStack;
    for(; state_count -- > 0;)
    {
        auto stmodel = new StateModel {*this, stack, &scenario};
        scenario.states.add(stmodel);
    }

    int32_t cmt_count;
    m_stream >> cmt_count;

    for(; cmt_count -- > 0 ;)
    {
        auto cmtModel = new CommentBlockModel {*this, &scenario};
        scenario.comments.add(cmtModel);
    }

    checkDelimiter();
}




template<>
void Visitor<Reader<JSONObject>>::readFrom(const Scenario::ScenarioModel& scenario)
{
    m_obj["PluginsMetadata"] = toJsonValue(*scenario.pluginModelList);
    m_obj["Metadata"] = toJsonObject(scenario.metadata);

    m_obj["StartTimeNodeId"] = toJsonValue(scenario.m_startTimeNodeId);
    m_obj["EndTimeNodeId"] = toJsonValue(scenario.m_endTimeNodeId);
    m_obj["StartEventId"] = toJsonValue(scenario.m_startEventId);
    m_obj["EndEventId"] = toJsonValue(scenario.m_endEventId);
    m_obj["StartStateId"] = toJsonValue(scenario.m_startStateId);

    m_obj["TimeNodes"] = toJsonArray(scenario.timeNodes);
    m_obj["Events"] = toJsonArray(scenario.events);
    m_obj["States"] = toJsonArray(scenario.states);
    m_obj["Constraints"] = toJsonArray(scenario.constraints);
    m_obj["Comments"] = toJsonArray(scenario.comments);
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(Scenario::ScenarioModel& scenario)
{
    Deserializer<JSONValue> elementPluginDeserializer(m_obj["PluginsMetadata"]);
    scenario.pluginModelList = new iscore::ElementPluginModelList{elementPluginDeserializer, &scenario};
    scenario.metadata = fromJsonObject<ModelMetadata>(m_obj["Metadata"].toObject());

    scenario.m_startTimeNodeId = fromJsonValue<Id<TimeNodeModel>> (m_obj["StartTimeNodeId"]);
    scenario.m_endTimeNodeId = fromJsonValue<Id<TimeNodeModel>> (m_obj["EndTimeNodeId"]);
    scenario.m_startEventId = fromJsonValue<Id<EventModel>> (m_obj["StartEventId"]);
    scenario.m_endEventId = fromJsonValue<Id<EventModel>> (m_obj["EndEventId"]);
    scenario.m_startStateId = fromJsonValue<Id<StateModel>> (m_obj["StartStateId"]);

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

    for(const auto& json_vref : m_obj["Comments"].toArray())
    {
        auto cmtmodel = new CommentBlockModel {
                       Deserializer<JSONObject>{json_vref.toObject() },
                       &scenario};

        scenario.comments.add(cmtmodel);
    }

    auto& stack = iscore::IDocument::documentContext(scenario).commandStack;
    for(const auto& json_vref : m_obj["States"].toArray())
    {
        auto stmodel = new StateModel {
                       Deserializer<JSONObject>{json_vref.toObject() },
                       stack,
                       &scenario};

        scenario.states.add(stmodel);
    }

}


void Scenario::ScenarioModel::serialize(const VisitorVariant& vis) const
{
    serialize_dyn(vis, *this);
}

Process* ScenarioFactory::loadModel(
        const VisitorVariant& vis,
        QObject* parent)
{
    return deserialize_dyn(vis, [&] (auto&& deserializer)
    { return new Scenario::ScenarioModel{deserializer, parent};});
}

LayerModel* Scenario::ScenarioModel::loadLayer_impl(
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
