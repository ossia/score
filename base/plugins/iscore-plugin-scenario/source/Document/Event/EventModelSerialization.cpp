#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include "source/Document/Event/EventModel.hpp"


template<> void Visitor<Reader<DataStream>>::readFrom(const EventModel& ev)
{
    readFrom(static_cast<const IdentifiedObject<EventModel>&>(ev));

    readFrom(ev.metadata);

    m_stream << ev.m_timeNode
             << ev.m_states
             << ev.m_condition
             << ev.m_date
             << ev.m_trigger;

    readFrom(ev.pluginModelList);

    insertDelimiter();
}

template<> void Visitor<Writer<DataStream>>::writeTo(EventModel& ev)
{
    writeTo(ev.metadata);

    m_stream >> ev.m_timeNode
             >> ev.m_states
             >> ev.m_condition
             >> ev.m_date
             >> ev.m_trigger;

    ev.pluginModelList = iscore::ElementPluginModelList{*this, &ev};

    checkDelimiter();
}




template<> void Visitor<Reader<JSONObject>>::readFrom(const EventModel& ev)
{
    readFrom(static_cast<const IdentifiedObject<EventModel>&>(ev));
    m_obj["Metadata"] = toJsonObject(ev.metadata);

    m_obj["TimeNode"] = toJsonValue(ev.m_timeNode);
    m_obj["States"] = toJsonArray(ev.m_states);
    m_obj["Condition"] = ev.m_condition;
    m_obj["Date"] = toJsonValue(ev.m_date);
    m_obj["Trigger"] = ev.m_trigger;

    m_obj["PluginsMetadata"] = toJsonValue(ev.pluginModelList);
}

template<> void Visitor<Writer<JSONObject>>::writeTo(EventModel& ev)
{
    ev.metadata = fromJsonObject<ModelMetadata>(m_obj["Metadata"].toObject());

    ev.m_timeNode = fromJsonValue<id_type<TimeNodeModel>> (m_obj["TimeNode"]);
    fromJsonValueArray(m_obj["States"].toArray(), ev.m_states);
    ev.m_condition = m_obj["Condition"].toString();
    ev.m_date = fromJsonValue<TimeValue> (m_obj["Date"]);
    ev.m_trigger = m_obj["Trigger"].toString();

    Deserializer<JSONValue> elementPluginDeserializer(m_obj["PluginsMetadata"]);
    ev.pluginModelList = iscore::ElementPluginModelList{elementPluginDeserializer, &ev};
}
