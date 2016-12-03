#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

#include "LocalSpecificSettings.hpp"

template <typename T>
class Reader;
template <typename T>
class Writer;

template <>
void Visitor<Reader<DataStream>>::readFrom_impl(
    const Engine::Network::LocalSpecificSettings& n)
{
  m_stream << n.remoteName << n.host << n.remotePort << n.localPort;
  insertDelimiter();
}

template <>
void Visitor<Writer<DataStream>>::writeTo(
    Engine::Network::LocalSpecificSettings& n)
{
  m_stream >> n.remoteName >> n.host >> n.remotePort >> n.localPort;
  checkDelimiter();
}

template <>
void Visitor<Reader<JSONObject>>::readFrom_impl(
    const Engine::Network::LocalSpecificSettings& n)
{
  m_obj["RemoteName"] = n.remoteName;
  m_obj["Host"] = n.host;
  m_obj["InPort"] = n.remotePort;
  m_obj["OutPort"] = n.localPort;
}

template <>
void Visitor<Writer<JSONObject>>::writeTo(
    Engine::Network::LocalSpecificSettings& n)
{
  n.remoteName = m_obj["RemoteName"].toString();
  n.host = m_obj["Host"].toString();
  n.remotePort = m_obj["InPort"].toInt();
  n.localPort = m_obj["OutPort"].toInt();
}
