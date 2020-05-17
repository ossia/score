// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "OSCSpecificSettings.hpp"

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>


template <>
void DataStreamReader::read(const Protocols::OSCSpecificSettings& n)
{
  // TODO put it in the right order before 1.0 final.
  // TODO same for minuit, etc..
  m_stream << n.outputPort << n.inputPort << n.host << n.rate;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Protocols::OSCSpecificSettings& n)
{
  m_stream >> n.outputPort >> n.inputPort >> n.host >> n.rate;
  checkDelimiter();
}

template <>
void JSONReader::read(const Protocols::OSCSpecificSettings& n)
{
  obj["OutputPort"] = n.outputPort;
  obj["InputPort"] = n.inputPort;
  obj["Host"] = n.host;
  if (n.rate)
    obj["Rate"] = *n.rate;
}

template <>
void JSONWriter::write(Protocols::OSCSpecificSettings& n)
{
  n.outputPort = obj["OutputPort"].toInt();
  n.inputPort = obj["InputPort"].toInt();
  n.host = obj["Host"].toString();
  if (auto it = obj.tryGet("Rate"))
    n.rate = it->toInt();
}
