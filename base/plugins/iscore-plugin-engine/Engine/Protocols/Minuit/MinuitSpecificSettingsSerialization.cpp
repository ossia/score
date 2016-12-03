#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

#include "MinuitSpecificSettings.hpp"

template <typename T>
class Reader;
template <typename T>
class Writer;

template <>
void Visitor<Reader<DataStream>>::readFrom_impl(
    const Engine::Network::MinuitSpecificSettings& n)
{
  m_stream << n.host << n.inputPort << n.outputPort;
  insertDelimiter();
}

template <>
void Visitor<Writer<DataStream>>::writeTo(
    Engine::Network::MinuitSpecificSettings& n)
{
  m_stream >> n.host >> n.inputPort >> n.outputPort;
  checkDelimiter();
}

template <>
void Visitor<Reader<JSONObject>>::readFrom_impl(
    const Engine::Network::MinuitSpecificSettings& n)
{
  m_obj["InPort"] = n.inputPort;
  m_obj["OutPort"] = n.outputPort;
  m_obj["Host"] = n.host;
}

template <>
void Visitor<Writer<JSONObject>>::writeTo(
    Engine::Network::MinuitSpecificSettings& n)
{
  n.inputPort = m_obj["InPort"].toInt();
  n.outputPort = m_obj["OutPort"].toInt();
  n.host = m_obj["Host"].toString();
}
