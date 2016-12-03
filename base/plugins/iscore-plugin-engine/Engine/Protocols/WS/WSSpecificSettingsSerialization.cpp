#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

#include "WSSpecificSettings.hpp"

template <>
void Visitor<Reader<DataStream>>::readFrom_impl(
    const Engine::Network::WSSpecificSettings& n)
{
  m_stream << n.address << n.text;
  insertDelimiter();
}

template <>
void Visitor<Writer<DataStream>>::writeTo(
    Engine::Network::WSSpecificSettings& n)
{
  m_stream >> n.address >> n.text;
  checkDelimiter();
}

template <>
void Visitor<Reader<JSONObject>>::readFrom_impl(
    const Engine::Network::WSSpecificSettings& n)
{
  m_obj[strings.Address] = n.address;
  m_obj["Text"] = n.text;
}

template <>
void Visitor<Writer<JSONObject>>::writeTo(
    Engine::Network::WSSpecificSettings& n)
{
  n.address = m_obj[strings.Address].toString();
  n.text = m_obj["Text"].toString();
}
