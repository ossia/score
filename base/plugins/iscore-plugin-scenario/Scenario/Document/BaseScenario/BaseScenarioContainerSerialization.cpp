#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <QJsonObject>
#include <QJsonValue>
#include <algorithm>

#include "BaseScenarioContainer.hpp"
#include <iscore/document/DocumentContext.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

template <typename T> class Reader;
template <typename T> class Writer;


template<> void Visitor<Reader<DataStream>>::readFrom(const BaseScenarioContainer& base_scenario)
{
    readFrom(*base_scenario.m_constraint);

    readFrom(*base_scenario.m_startNode);
    readFrom(*base_scenario.m_endNode);

    readFrom(*base_scenario.m_startEvent);
    readFrom(*base_scenario.m_endEvent);

    readFrom(*base_scenario.m_startState);
    readFrom(*base_scenario.m_endState);
}

template<> void Visitor<Writer<DataStream>>::writeTo(BaseScenarioContainer& base_scenario)
{
    base_scenario.m_constraint = new ConstraintModel{*this, base_scenario.m_parent};

    base_scenario.m_startNode = new TimeNodeModel{*this, base_scenario.m_parent};
    base_scenario.m_endNode = new TimeNodeModel{*this, base_scenario.m_parent};

    base_scenario.m_startEvent = new EventModel{*this, base_scenario.m_parent};
    base_scenario.m_endEvent = new EventModel{*this, base_scenario.m_parent};

    auto& stack = iscore::IDocument::documentContext(base_scenario.parent()).commandStack;
    base_scenario.m_startState = new StateModel{*this, stack, base_scenario.m_parent};
    base_scenario.m_endState = new StateModel{*this, stack, base_scenario.m_parent};
}


template<> void Visitor<Reader<JSONObject>>::readFrom(const BaseScenarioContainer& base_scenario)
{
    m_obj["Constraint"] = toJsonObject(*base_scenario.m_constraint);

    m_obj["StartTimeNode"] = toJsonObject(*base_scenario.m_startNode);
    m_obj["EndTimeNode"] = toJsonObject(*base_scenario.m_endNode);

    m_obj["StartEvent"] = toJsonObject(*base_scenario.m_startEvent);
    m_obj["EndEvent"] = toJsonObject(*base_scenario.m_endEvent);

    m_obj["StartState"] = toJsonObject(*base_scenario.m_startState);
    m_obj["EndState"] = toJsonObject(*base_scenario.m_endState);
}


template<> void Visitor<Writer<JSONObject>>::writeTo(BaseScenarioContainer& base_scenario)
{
    base_scenario.m_constraint = new ConstraintModel{
                                 Deserializer<JSONObject>{m_obj["Constraint"].toObject() },
                                 base_scenario.m_parent};

    base_scenario.m_startNode = new TimeNodeModel{
                                Deserializer<JSONObject>{m_obj["StartTimeNode"].toObject() },
                                base_scenario.m_parent};
    base_scenario.m_endNode = new TimeNodeModel{
                            Deserializer<JSONObject>{m_obj["EndTimeNode"].toObject() },
                            base_scenario.m_parent};

    base_scenario.m_startEvent = new EventModel{
                                Deserializer<JSONObject>{m_obj["StartEvent"].toObject() },
                                base_scenario.m_parent};
    base_scenario.m_endEvent = new EventModel{
                            Deserializer<JSONObject>{m_obj["EndEvent"].toObject() },
                            base_scenario.m_parent};

    auto& stack = iscore::IDocument::documentContext(base_scenario.parent()).commandStack;
    base_scenario.m_startState = new StateModel{
                                Deserializer<JSONObject>{m_obj["StartState"].toObject() },
                                stack,
                                base_scenario.m_parent};
    base_scenario.m_endState = new StateModel{
                            Deserializer<JSONObject>{m_obj["EndState"].toObject() },
                            stack,
                            base_scenario.m_parent};
}
