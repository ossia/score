#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include "RecordedMessagesProcessModel.hpp"
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
    m_obj[iscore::StringConstant().Message] = toJsonObject(rm.message);
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(
        RecordedMessages::RecordedMessage& rm)
{
    rm.percentage = m_obj["Percentage"].toDouble();
    rm.message = fromJsonObject<State::Message>(m_obj[iscore::StringConstant().Message]);
}



template<>
void Visitor<Reader<DataStream>>::readFrom_impl(const RecordedMessages::ProcessModel& proc)
{
    m_stream << proc.m_messages;

    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(RecordedMessages::ProcessModel& proc)
{
    m_stream >> proc.m_messages;

    checkDelimiter();
}

template<>
void Visitor<Reader<JSONObject>>::readFrom_impl(const RecordedMessages::ProcessModel& proc)
{
    m_obj["Messages"] = toJsonArray(proc.messages());
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(RecordedMessages::ProcessModel& proc)
{
    fromJsonArray(m_obj["Messages"].toArray(), proc.m_messages);
}
