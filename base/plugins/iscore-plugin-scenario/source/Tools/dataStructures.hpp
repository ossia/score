#pragma once

/*
This file is used to define simple data structure to simplify the code when needed
*/

#include <QVector>
#include <iscore/tools/SettableIdentifier.hpp>
#include <ProcessInterface/TimeValue.hpp>

class TimeNodeModel;
class ConstraintModel;

struct TimenodeProperties {
    TimeValue oldDate;
    TimeValue newDate;
};

struct ConstraintProperties {
    TimeValue oldMin;
    TimeValue newMin;
    TimeValue oldMax;
    TimeValue newMax;
};

struct ElementsProperties {
    QMap<Id<TimeNodeModel>, TimenodeProperties> timenodes;
    QMap<Id<ConstraintModel>, ConstraintProperties> constraints;
};
/*
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

template<>
void Visitor<Reader<DataStream>>::readFrom(const ElementsProperties& timenode)
{
   // m_stream << timenode.id << timenode.oldDate << ...

    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(ElementsProperties& timenode)
{

//    m_stream >> timenode.m_date
//             >> timenode.m_events;


    checkDelimiter();
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const TimeNodeModel& timenode)
{
    readFrom(static_cast<const IdentifiedObject<TimeNodeModel>&>(timenode));
    m_obj["Metadata"] = toJsonObject(timenode.metadata);

    m_obj["Date"] = toJsonValue(timenode.date());
    m_obj["Events"] = toJsonArray(timenode.m_events);

    m_obj["PluginsMetadata"] = toJsonValue(timenode.pluginModelList);
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(TimeNodeModel& timenode)
{
    timenode.metadata = fromJsonObject<ModelMetadata>(m_obj["Metadata"].toObject());

    timenode.m_date = fromJsonValue<TimeValue> (m_obj["Date"]);

    fromJsonValueArray(m_obj["Events"].toArray(), timenode.m_events);

    Deserializer<JSONValue> elementPluginDeserializer(m_obj["PluginsMetadata"]);
    timenode.pluginModelList = iscore::ElementPluginModelList{elementPluginDeserializer, &timenode};
}
*/
