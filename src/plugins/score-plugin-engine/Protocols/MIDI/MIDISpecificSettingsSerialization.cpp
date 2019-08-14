// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MIDISpecificSettings.hpp"

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONValueVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>


template <>
void DataStreamReader::read(const Protocols::MIDISpecificSettings& n)
{
  m_stream << n.io << n.endpoint << n.port << n.createWholeTree;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Protocols::MIDISpecificSettings& n)
{
  m_stream >> n.io >> n.endpoint >> n.port >> n.createWholeTree;
  checkDelimiter();
}

template <>
void JSONObjectReader::read(const Protocols::MIDISpecificSettings& n)
{
  obj["IO"] = toJsonValue(n.io);
  obj["Endpoint"] = n.endpoint;
  obj["Port"] = (int)n.port;
  obj["CreateWholeTree"] = n.createWholeTree;
}

template <>
void JSONObjectWriter::write(Protocols::MIDISpecificSettings& n)
{
  fromJsonValue(obj["IO"], n.io);
  n.endpoint = obj["Endpoint"].toString();
  n.port = obj["Port"].toInt();
  if (obj.contains("CreateWholeTree"))
    n.createWholeTree = obj["CreateWholeTree"].toBool();
  else
    n.createWholeTree = true;
}
