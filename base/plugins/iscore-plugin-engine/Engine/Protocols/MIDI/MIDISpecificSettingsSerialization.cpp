#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

#include "MIDISpecificSettings.hpp"
#include <iscore/serialization/JSONValueVisitor.hpp>


template <>
void DataStreamReader::read(
    const Engine::Network::MIDISpecificSettings& n)
{
  m_stream << n.io << n.endpoint << n.port;
  insertDelimiter();
}


template <>
void DataStreamWriter::write(
    Engine::Network::MIDISpecificSettings& n)
{
  m_stream >> n.io >> n.endpoint >> n.port;
  checkDelimiter();
}


template <>
void JSONObjectReader::read(
    const Engine::Network::MIDISpecificSettings& n)
{
  obj["IO"] = toJsonValue(n.io);
  obj["Endpoint"] = n.endpoint;
  obj["Port"] = (int)n.port;
}


template <>
void JSONObjectWriter::write(
    Engine::Network::MIDISpecificSettings& n)
{
  fromJsonValue(obj["IO"], n.io);
  n.endpoint = obj["Endpoint"].toString();
  n.port = obj["Port"].toInt();
}
