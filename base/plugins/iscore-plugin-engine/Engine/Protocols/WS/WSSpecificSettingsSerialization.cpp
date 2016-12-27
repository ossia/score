#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

#include "WSSpecificSettings.hpp"


template <>
void DataStreamReader::read(
    const Engine::Network::WSSpecificSettings& n)
{
  m_stream << n.address << n.text;
  insertDelimiter();
}


template <>
void DataStreamWriter::writeTo(
    Engine::Network::WSSpecificSettings& n)
{
  m_stream >> n.address >> n.text;
  checkDelimiter();
}


template <>
void JSONObjectReader::readFromConcrete(
    const Engine::Network::WSSpecificSettings& n)
{
  obj[strings.Address] = n.address;
  obj["Text"] = n.text;
}


template <>
void JSONObjectWriter::writeTo(
    Engine::Network::WSSpecificSettings& n)
{
  n.address = obj[strings.Address].toString();
  n.text = obj["Text"].toString();
}
