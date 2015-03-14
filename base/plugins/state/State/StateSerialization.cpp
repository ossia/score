#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include "State.hpp"


template<>
void Visitor<Reader<DataStream>>::readFrom(const State& mess)
{
    m_stream << mess.m_data;
    insertDelimiter();
}

template<>
void Visitor<Reader<JSON>>::readFrom(const State& mess)
{
    QVariantMap vm;
    vm["Data"] = mess.m_data;
    m_obj["State"] = QJsonObject::fromVariantMap(vm);
}

template<>
void Visitor<Writer<DataStream>>::writeTo(State& mess)
{
    m_stream >> mess.m_data;

    checkDelimiter();
}

template<>
void Visitor<Writer<JSON>>::writeTo(State& mess)
{
    mess.m_data = m_obj["State"].toObject().toVariantMap()["Data"];
}

#include "Message.hpp"

template<>
void Visitor<Reader<DataStream>>::readFrom(const Message& mess)
{
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

