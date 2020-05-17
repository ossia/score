// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "LocalSpecificSettings.hpp"

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

template <>
void DataStreamReader::read(const Protocols::LocalSpecificSettings& n)
{
  m_stream << n.wsPort << n.oscPort;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Protocols::LocalSpecificSettings& n)
{
  m_stream >> n.wsPort >> n.oscPort;
  checkDelimiter();
}

template <>
void JSONReader::read(const Protocols::LocalSpecificSettings& n)
{
  obj["WSPort"] = n.wsPort;
  obj["OSCPort"] = n.oscPort;
}

template <>
void JSONWriter::write(Protocols::LocalSpecificSettings& n)
{
  n.wsPort = obj["WSPort"].toInt();
  n.oscPort = obj["OSCPort"].toInt();
}
