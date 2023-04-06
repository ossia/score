// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "OSCQuerySpecificSettings.hpp"

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

template <>
void DataStreamReader::read(const Protocols::OSCQuerySpecificSettings& n)
{
  m_stream << n.host << n.rate << n.localPort;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Protocols::OSCQuerySpecificSettings& n)
{
  m_stream >> n.host >> n.rate >> n.localPort;
  checkDelimiter();
}

template <>
void JSONReader::read(const Protocols::OSCQuerySpecificSettings& n)
{
  obj["Host"] = n.host;
  if(n.rate)
    obj["Rate"] = *n.rate;

  obj["LocalPort"] = n.localPort;
}

template <>
void JSONWriter::write(Protocols::OSCQuerySpecificSettings& n)
{
  n.host = obj["Host"].toString();
  if(auto it = obj.tryGet("Rate"))
    n.rate = it->toInt();
  if(auto it = obj.tryGet("LocalPort"))
    n.localPort = it->toInt();
}
