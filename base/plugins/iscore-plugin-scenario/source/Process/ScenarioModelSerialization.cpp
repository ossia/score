#include "Document/Event/EventModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/TimeNode/TimeNodeModel.hpp"
#include "Process/Temporal/TemporalScenarioViewModel.hpp"
#include "ScenarioModel.hpp"
#include <iscore/serialization/JSONValueVisitor.hpp>

template<>
void Visitor<Reader<DataStream>>::readFrom(const ScenarioModel& scenario)
{
    m_stream << scenario.m_startEventId;
    m_stream << scenario.m_endEventId;

    // Constraints
    const auto& constraints = scenario.constraints();
    m_stream << (int) constraints.size();

    for(const auto& constraint : constraints)
    {
        readFrom(*constraint);
    }

    // Events
    const auto& events = scenario.events();
    m_stream << (int) events.size();

    for(const auto& event : events)
    {
        readFrom(*event);
    }

    // Timenodes
    const auto& timenodes = scenario.timeNodes();
    m_stream << (int) timenodes.size();

    for(const auto& timenode : timenodes)
    {
        readFrom(*timenode);
    }

    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(ScenarioModel& scenario)
{
    m_stream >> scenario.m_startEventId;
    m_stream >> scenario.m_endEventId;

    // Constraints
    int constraint_count;
    m_stream >> constraint_count;

    for(; constraint_count -- > 0;)
    {
        auto constraint = new ConstraintModel {*this, &scenario};
        scenario.addConstraint(constraint);
    }

    // Events
    int event_count;
    m_stream >> event_count;

    for(; event_count -- > 0;)
    {
        auto evmodel = new EventModel {*this, &scenario};
        scenario.addEvent(evmodel);
    }

    // Timenodes
    int timenode_count;
    m_stream >> timenode_count;

    for(; timenode_count -- > 0;)
    {
        auto tnmodel = new TimeNodeModel {*this, &scenario};
        scenario.addTimeNode(tnmodel);
    }

    checkDelimiter();
}




template<>
void Visitor<Reader<JSONObject>>::readFrom(const ScenarioModel& scenario)
{
    m_obj["StartEventId"] = toJsonValue(scenario.m_startEventId);
    m_obj["EndEventId"] = toJsonValue(scenario.m_endEventId);

    m_obj["Constraints"] = toJsonArray(scenario.constraints());
    m_obj["Events"] = toJsonArray(scenario.events());
    m_obj["TimeNodes"] = toJsonArray(scenario.timeNodes());
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(ScenarioModel& scenario)
{
    scenario.m_startEventId = fromJsonValue<id_type<EventModel>> (m_obj["StartEventId"]);
    scenario.m_endEventId = fromJsonValue<id_type<EventModel>> (m_obj["EndEventId"]);

    for(const auto& json_vref : m_obj["Constraints"].toArray())
    {
        auto constraint = new ConstraintModel{
                Deserializer<JSONObject>{json_vref.toObject() },
                &scenario};
        scenario.addConstraint(constraint);
    }

    for(const auto& json_vref : m_obj["Events"].toArray())
    {
        auto evmodel = new EventModel {
                       Deserializer<JSONObject>{json_vref.toObject() },
                       &scenario};

        scenario.addEvent(evmodel);
    }

    for(const auto& json_vref : m_obj["TimeNodes"].toArray())
    {
        auto tnmodel = new TimeNodeModel {
                       Deserializer<JSONObject>{json_vref.toObject() },
                       &scenario};

        scenario.addTimeNode(tnmodel);
    }
}


#include <iscore/serialization/VisitorCommon.hpp>
void ScenarioModel::serialize(const VisitorVariant& vis) const
{
    serialize_dyn(vis, *this);
}

#include "ScenarioFactory.hpp"
ProcessModel* ScenarioFactory::loadModel(
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
        auto scen = new TemporalScenarioViewModel{
                            deserializer, *this, parent};
        this->makeLayer_impl(scen);
        return scen;
    });
}
