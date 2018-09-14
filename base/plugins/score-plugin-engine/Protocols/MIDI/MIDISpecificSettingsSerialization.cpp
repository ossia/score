// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MIDISpecificSettings.hpp"

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONValueVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <QJsonObject>
#include <QJsonValue>
#include <QString>

template <>
void DataStreamReader::read(const Engine::Network::MIDISpecificSettings& n)
{
  m_stream << n.io << n.endpoint << n.port;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Engine::Network::MIDISpecificSettings& n)
{
  m_stream >> n.io >> n.endpoint >> n.port;
  checkDelimiter();
}

template <>
void JSONObjectReader::read(const Engine::Network::MIDISpecificSettings& n)
{
  obj["IO"] = toJsonValue(n.io);
  obj["Endpoint"] = n.endpoint;
  obj["Port"] = (int)n.port;
}

template <>
void JSONObjectWriter::write(Engine::Network::MIDISpecificSettings& n)
{
  fromJsonValue(obj["IO"], n.io);
  n.endpoint = obj["Endpoint"].toString();
  n.port = obj["Port"].toInt();
}
