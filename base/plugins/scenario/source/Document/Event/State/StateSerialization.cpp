#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include "State.hpp"

template<>
void Visitor<Reader<DataStream>>::readFrom(const State& state)
{
    readFrom(static_cast<const IdentifiedObject<State>&>(state));

    m_stream << state.messages();

    insertDelimiter();
}

template<>
void Visitor<Reader<JSON>>::readFrom(const State& state)
{
    readFrom(static_cast<const IdentifiedObject<State>&>(state));

    m_obj["Messages"] = QJsonArray::fromStringList(state.messages());
}

template<>
void Visitor<Writer<DataStream>>::writeTo(State& state)
{
    QStringList messages;
    m_stream >> messages;

    for(auto& message : messages)
    {
        state.addMessage(message);
    }

    checkDelimiter();
}

template<>
void Visitor<Writer<JSON>>::writeTo(State& state)
{
    for(auto& message : m_obj["Messages"].toArray().toVariantList())
    {
        state.addMessage(message.toString());
    }
}





#include "Message.hpp"

template<>
void Visitor<Reader<DataStream>>::readFrom(const Message& mess)
{
    // TODO Continue / overhaul messages.
    m_stream << mess.address << mess.value;
    insertDelimiter();
}

template<>
void Visitor<Reader<JSON>>::readFrom(const Message& mess)
{
    m_obj["Address"] = mess.address;
    m_obj["Value"] = mess.value.toString();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(Message& mess)
{
    m_stream >> mess.address >> mess.value;

    checkDelimiter();
}

template<>
void Visitor<Writer<JSON>>::writeTo(Message& mess)
{
    mess.address = m_obj["Address"].toString();
    mess.value = QJsonValue(m_obj["Value"]).toVariant();
}

