#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

#include "LocalSpecificSettings.hpp"


template <>
void DataStreamReader::read(
    const Engine::Network::LocalSpecificSettings& n)
{
  m_stream << n.remoteName << n.host << n.remotePort << n.localPort;
  insertDelimiter();
}


template <>
void DataStreamWriter::writeTo(
    Engine::Network::LocalSpecificSettings& n)
{
  m_stream >> n.remoteName >> n.host >> n.remotePort >> n.localPort;
  checkDelimiter();
}


template <>
void JSONObjectReader::read(
    const Engine::Network::LocalSpecificSettings& n)
{
  obj["RemoteName"] = n.remoteName;
  obj["Host"] = n.host;
  obj["InPort"] = n.remotePort;
  obj["OutPort"] = n.localPort;
}


template <>
void JSONObjectWriter::writeTo(
    Engine::Network::LocalSpecificSettings& n)
{
  n.remoteName = obj["RemoteName"].toString();
  n.host = obj["Host"].toString();
  n.remotePort = obj["InPort"].toInt();
  n.localPort = obj["OutPort"].toInt();
}
