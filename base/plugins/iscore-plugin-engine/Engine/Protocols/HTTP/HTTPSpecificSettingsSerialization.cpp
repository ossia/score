#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include "HTTPSpecificSettings.hpp"

template <typename T> class Reader;
template <typename T> class Writer;


template<>
void Visitor<Reader<DataStream>>::readFrom_impl(const Engine::Network::HTTPSpecificSettings& n)
{
    m_stream << n.text;
    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(Engine::Network::HTTPSpecificSettings& n)
{
    m_stream >> n.text;
    checkDelimiter();
}

template<>
void Visitor<Reader<JSONObject>>::readFrom_impl(const Engine::Network::HTTPSpecificSettings& n)
{
    m_obj["Text"] = n.text;
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(Engine::Network::HTTPSpecificSettings& n)
{
    n.text = m_obj["Text"].toString();
}
