#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include "RecordedMessagesProcessModel.hpp"
#include <iscore/plugins/documentdelegate/plugin/ElementPluginModelList.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

template <typename T> class Reader;
template <typename T> class Writer;

template<>
void Visitor<Reader<DataStream>>::readFrom(
        const RecordedMessages::RecordedMessage& rm)
{
    m_stream << rm.percentage << rm.message;
}

template<>
void Visitor<Writer<DataStream>>::writeTo(
        RecordedMessages::RecordedMessage& rm)
{
    m_stream >> rm.percentage >> rm.message;
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(
        const RecordedMessages::RecordedMessage& rm)
{
    m_obj["Percentage"] = rm.percentage;
    m_obj["Message"] = toJsonObject(rm.message);
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(
        RecordedMessages::RecordedMessage& rm)
{
    rm.percentage = m_obj["Percentage"].toDouble();
    rm.message = fromJsonObject<State::Message>(m_obj["Message"].toObject());
}



template<>
void Visitor<Reader<DataStream>>::readFrom_impl(const RecordedMessages::ProcessModel& proc)
{
    readFrom(*proc.pluginModelList);

    m_stream << proc.m_messages;

    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(RecordedMessages::ProcessModel& proc)
{
    proc.pluginModelList = new iscore::ElementPluginModelList{*this, &proc};

    m_stream >> proc.m_messages;

    checkDelimiter();
}

template<>
void Visitor<Reader<JSONObject>>::readFrom_impl(const RecordedMessages::ProcessModel& proc)
{
    m_obj["PluginsMetadata"] = toJsonValue(*proc.pluginModelList);
    m_obj["Messages"] = toJsonArray(proc.messages());
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(RecordedMessages::ProcessModel& proc)
{
    Deserializer<JSONValue> elementPluginDeserializer(m_obj["PluginsMetadata"]);
    proc.pluginModelList = new iscore::ElementPluginModelList{elementPluginDeserializer, &proc};

    fromJsonArray(m_obj["Messages"].toArray(), proc.m_messages);
}
