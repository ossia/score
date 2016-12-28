#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

#include "OSCSpecificSettings.hpp"



template <>
void DataStreamReader::read(
    const Engine::Network::OSCSpecificSettings& n)
{
  // TODO put it in the right order before 1.0 final.
  // TODO same for minuit, etc..
  m_stream << n.outputPort << n.inputPort << n.host;
  insertDelimiter();
}


template <>
void DataStreamWriter::write(
    Engine::Network::OSCSpecificSettings& n)
{
  m_stream >> n.outputPort >> n.inputPort >> n.host;
  checkDelimiter();
}


template <>
void JSONObjectReader::read(
    const Engine::Network::OSCSpecificSettings& n)
{
  obj["OutputPort"] = n.outputPort;
  obj["InputPort"] = n.inputPort;
  obj["Host"] = n.host;
}


template <>
void JSONObjectWriter::write(
    Engine::Network::OSCSpecificSettings& n)
{
  n.outputPort = obj["OutputPort"].toInt();
  n.inputPort = obj["InputPort"].toInt();
  n.host = obj["Host"].toString();
}
