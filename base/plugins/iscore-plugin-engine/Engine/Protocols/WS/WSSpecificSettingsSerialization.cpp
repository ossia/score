// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
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
void DataStreamWriter::write(
    Engine::Network::WSSpecificSettings& n)
{
  m_stream >> n.address >> n.text;
  checkDelimiter();
}


template <>
void JSONObjectReader::read(
    const Engine::Network::WSSpecificSettings& n)
{
  obj[strings.Address] = n.address;
  obj["Text"] = n.text;
}


template <>
void JSONObjectWriter::write(
    Engine::Network::WSSpecificSettings& n)
{
  n.address = obj[strings.Address].toString();
  n.text = obj["Text"].toString();
}
