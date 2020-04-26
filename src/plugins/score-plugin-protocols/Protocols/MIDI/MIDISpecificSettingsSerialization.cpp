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
void JSONReader::read(const Protocols::MIDISpecificSettings& n)
{
  obj["IO"] = n.io;
  obj["Endpoint"] = n.endpoint;
  obj["Port"] = (int)n.port;
  obj["CreateWholeTree"] = n.createWholeTree;
}

template <>
void JSONWriter::write(Protocols::MIDISpecificSettings& n)
{
  obj["IO"], n.io;
  n.endpoint = obj["Endpoint"].toString();
  n.port = obj["Port"].toInt();
  if (auto it = obj.tryGet("CreateWholeTree"))
    n.createWholeTree = it->toBool();
  else
    n.createWholeTree = true;
}
