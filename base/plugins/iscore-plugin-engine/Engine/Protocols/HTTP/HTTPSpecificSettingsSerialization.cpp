#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

#include "HTTPSpecificSettings.hpp"


template <>
void DataStreamReader::read(
    const Engine::Network::HTTPSpecificSettings& n)
{
  m_stream << n.text;
  insertDelimiter();
}


template <>
void DataStreamWriter::writeTo(
    Engine::Network::HTTPSpecificSettings& n)
{
  m_stream >> n.text;
  checkDelimiter();
}


template <>
void JSONObjectReader::readFromConcrete(
    const Engine::Network::HTTPSpecificSettings& n)
{
  obj["Text"] = n.text;
}


template <>
void JSONObjectWriter::writeTo(
    Engine::Network::HTTPSpecificSettings& n)
{
  n.text = obj["Text"].toString();
}
