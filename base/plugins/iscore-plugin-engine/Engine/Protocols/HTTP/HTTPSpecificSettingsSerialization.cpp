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
void DataStreamWriter::write(
    Engine::Network::HTTPSpecificSettings& n)
{
  m_stream >> n.text;
  checkDelimiter();
}


template <>
void JSONObjectReader::read(
    const Engine::Network::HTTPSpecificSettings& n)
{
  obj["Text"] = n.text;
}


template <>
void JSONObjectWriter::write(
    Engine::Network::HTTPSpecificSettings& n)
{
  n.text = obj["Text"].toString();
}
