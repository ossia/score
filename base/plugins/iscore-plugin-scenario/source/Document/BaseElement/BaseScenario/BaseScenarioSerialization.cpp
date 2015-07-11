#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include "BaseScenarioModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Event/EventModel.hpp"
#include "Document/TimeNode/TimeNodeModel.hpp"


template<> void Visitor<Reader<DataStream>>::readFrom(const BaseScenario& base_scenario)
{
    readFrom(static_cast<const IdentifiedObject<BaseScenario>&>(base_scenario));

    readFrom(*base_scenario.m_startNode);
    readFrom(*base_scenario.m_endNode);

    readFrom(*base_scenario.m_startEvent);
    readFrom(*base_scenario.m_endEvent);

    readFrom(*base_scenario.m_startState);
    readFrom(*base_scenario.m_endState);

    readFrom(*base_scenario.m_constraint);

    readFrom(base_scenario.pluginModelList);

    insertDelimiter();
}

template<> void Visitor<Writer<DataStream>>::writeTo(BaseScenario& base_scenario)
{
    base_scenario.m_startNode = new TimeNodeModel{*this, &base_scenario};
    base_scenario.m_endNode = new TimeNodeModel{*this, &base_scenario};

    base_scenario.m_startEvent = new EventModel{*this, &base_scenario};
    base_scenario.m_endEvent = new EventModel{*this, &base_scenario};

    base_scenario.m_startState = new StateModel{*this, &base_scenario};
    base_scenario.m_endState = new StateModel{*this, &base_scenario};

    base_scenario.m_constraint = new ConstraintModel{*this, &base_scenario};

    base_scenario.pluginModelList = iscore::ElementPluginModelList{*this, &base_scenario};

    checkDelimiter();
}


template<> void Visitor<Reader<JSONObject>>::readFrom(const BaseScenario& base_scenario)
{
    readFrom(static_cast<const IdentifiedObject<BaseScenario>&>(base_scenario));

    m_obj["StartTimeNode"] = toJsonObject(*base_scenario.m_startNode);
    m_obj["EndTimeNode"] = toJsonObject(*base_scenario.m_endNode);

    m_obj["StartEvent"] = toJsonObject(*base_scenario.m_startEvent);
    m_obj["EndEvent"] = toJsonObject(*base_scenario.m_endEvent);

    m_obj["StartState"] = toJsonObject(*base_scenario.m_startState);
    m_obj["EndState"] = toJsonObject(*base_scenario.m_endState);

    m_obj["Constraint"] = toJsonObject(*base_scenario.m_constraint);

    m_obj["PluginsMetadata"] = toJsonValue(base_scenario.pluginModelList);
}


template<> void Visitor<Writer<JSONObject>>::writeTo(BaseScenario& base_scenario)
{
    base_scenario.m_startNode = new TimeNodeModel{
                                Deserializer<JSONObject>{m_obj["StartTimeNode"].toObject() },
                                &base_scenario};
    base_scenario.m_endNode = new TimeNodeModel{
                            Deserializer<JSONObject>{m_obj["EndTimeNode"].toObject() },
                            &base_scenario};

    base_scenario.m_startEvent = new EventModel{
                                Deserializer<JSONObject>{m_obj["StartEvent"].toObject() },
                                &base_scenario};
    base_scenario.m_endEvent = new EventModel{
                            Deserializer<JSONObject>{m_obj["EndEvent"].toObject() },
                            &base_scenario};

    base_scenario.m_startState = new StateModel{
                                Deserializer<JSONObject>{m_obj["StartState"].toObject() },
                                &base_scenario};
    base_scenario.m_endState = new StateModel{
                            Deserializer<JSONObject>{m_obj["EndState"].toObject() },
                            &base_scenario};

    base_scenario.m_constraint = new ConstraintModel{
                                 Deserializer<JSONObject>{m_obj["Constraint"].toObject() },
                                 &base_scenario};

    Deserializer<JSONValue> elementPluginDeserializer(m_obj["PluginsMetadata"]);
    base_scenario.pluginModelList = iscore::ElementPluginModelList{elementPluginDeserializer, &base_scenario};
}
