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
  m_stream << n.scoreListeningPort << n.deviceListeningPort << n.host << n.rate << n.jsonToLoad;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Protocols::OSCSpecificSettings& n)
{
  m_stream >> n.scoreListeningPort >> n.deviceListeningPort >> n.host >> n.rate >> n.jsonToLoad;
  checkDelimiter();
}

template <>
void JSONReader::read(const Protocols::OSCSpecificSettings& n)
{
  obj["OutputPort"] = n.scoreListeningPort;
  obj["InputPort"] = n.deviceListeningPort;
  obj["Host"] = n.host;
  if (n.rate)
    obj["Rate"] = *n.rate;
}

template <>
void JSONWriter::write(Protocols::OSCSpecificSettings& n)
{
  n.scoreListeningPort = obj["OutputPort"].toInt();
  n.deviceListeningPort = obj["InputPort"].toInt();
  n.host = obj["Host"].toString();
  if (auto it = obj.tryGet("Rate"))
    n.rate = it->toInt();
}
