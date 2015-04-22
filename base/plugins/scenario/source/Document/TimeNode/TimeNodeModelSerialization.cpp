#include "TimeNodeModel.hpp"


template<>
void Visitor<Reader<DataStream>>::readFrom(const TimeNodeModel& timenode)
{
    readFrom(static_cast<const IdentifiedObject<TimeNodeModel>&>(timenode));

    m_stream << timenode.metadata;

    m_stream << timenode.m_topY
             << timenode.m_bottomY
             << timenode.m_date
             << timenode.m_y
             << timenode.m_events;

    readFrom(*timenode.m_pluginModelList);

    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(TimeNodeModel& timenode)
{
    m_stream >> timenode.metadata;

    m_stream >> timenode.m_topY
             >> timenode.m_bottomY
             >> timenode.m_date
             >> timenode.m_y
             >> timenode.m_events;

    timenode.m_pluginModelList = new iscore::ElementPluginModelList{*this, &timenode};

    checkDelimiter();
}

template<>
void Visitor<Reader<JSON>>::readFrom(const TimeNodeModel& timenode)
{
    readFrom(static_cast<const IdentifiedObject<TimeNodeModel>&>(timenode));
    m_obj["Metadata"] = toJsonObject(timenode.metadata);

    m_obj["Top"] = timenode.top();
    m_obj["Bottom"] = timenode.bottom();
    m_obj["Date"] = toJsonObject(timenode.date());
    m_obj["Y"] = timenode.y();
    m_obj["Events"] = toJsonArray(timenode.m_events);

    m_obj["PluginsMetadata"] = toJsonObject(*timenode.m_pluginModelList);
}

template<>
void Visitor<Writer<JSON>>::writeTo(TimeNodeModel& timenode)
{
    timenode.metadata = fromJsonObject<ModelMetadata>(m_obj["Metadata"].toObject());

    timenode.m_topY = m_obj["Top"].toDouble();
    timenode.m_bottomY = m_obj["Bottom"].toDouble();
    timenode.m_date = fromJsonObject<TimeValue> (m_obj["Date"].toObject());
    timenode.m_y = m_obj["Y"].toDouble();

    fromJsonArray(m_obj["Events"].toArray(), timenode.m_events);

    m_obj["PluginsMetadata"] = toJsonObject(*timenode.m_pluginModelList);
}
