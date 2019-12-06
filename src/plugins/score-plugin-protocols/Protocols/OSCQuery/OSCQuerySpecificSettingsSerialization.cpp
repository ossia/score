// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "OSCQuerySpecificSettings.hpp"

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>


template <>
void DataStreamReader::read(const Protocols::OSCQuerySpecificSettings& n)
{
  m_stream << n.host << n.rate;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Protocols::OSCQuerySpecificSettings& n)
{
  m_stream >> n.host >> n.rate;
  checkDelimiter();
}

template <>
void JSONObjectReader::read(const Protocols::OSCQuerySpecificSettings& n)
{
  obj["Host"] = n.host;
  if (n.rate)
    obj["Rate"] = *n.rate;
}

template <>
void JSONObjectWriter::write(Protocols::OSCQuerySpecificSettings& n)
{
  n.host = obj["Host"].toString();
  if (obj.contains("Rate"))
    n.rate = obj["Rate"].toInt();
}
