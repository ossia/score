#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

#include "OSCQuerySpecificSettings.hpp"


template <>
void DataStreamReader::read(
    const Engine::Network::OSCQuerySpecificSettings& n)
{
  m_stream << n.host;
  insertDelimiter();
}


template <>
void DataStreamWriter::write(
    Engine::Network::OSCQuerySpecificSettings& n)
{
  m_stream >> n.host;
  checkDelimiter();
}


template <>
void JSONObjectReader::read(
    const Engine::Network::OSCQuerySpecificSettings& n)
{
  obj["Host"] = n.host;
}


template <>
void JSONObjectWriter::write(
    Engine::Network::OSCQuerySpecificSettings& n)
{
  n.host = obj["Host"].toString();
}
