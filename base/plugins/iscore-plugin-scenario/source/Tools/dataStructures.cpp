#include "dataStructures.hpp"
#include <QVector>

#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

template<>
void Visitor<Reader<DataStream>>::readFrom(const TimenodeProperties& timenodeProperties)
{
    m_stream << timenodeProperties.oldDate
             << timenodeProperties.newDate;

    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(TimenodeProperties& timenodeProperties)
{

    m_stream >> timenodeProperties.oldDate
             >> timenodeProperties.newDate;

    checkDelimiter();
}

//----------

template<>
void Visitor<Reader<DataStream>>::readFrom(const ConstraintProperties& constraintProperties)
{
    m_stream << constraintProperties.oldMin
             << constraintProperties.newMin
             << constraintProperties.oldMax
             << constraintProperties.newMax;

    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(ConstraintProperties& constraintProperties)
{

    m_stream >> constraintProperties.oldMin
             >> constraintProperties.newMin
             >> constraintProperties.oldMax
             >> constraintProperties.newMax;

    checkDelimiter();
}

//----------
template<>
void Visitor<Reader<DataStream>>::readFrom(const ElementsProperties& elementsProperties)
{
    m_stream << elementsProperties.timenodes
             << elementsProperties.constraints;

    insertDelimiter();
}



template<>
void Visitor<Writer<DataStream>>::writeTo(ElementsProperties& elementsProperties)
{

    m_stream >> elementsProperties.timenodes
             >> elementsProperties.constraints;


    checkDelimiter();
}
/*
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
