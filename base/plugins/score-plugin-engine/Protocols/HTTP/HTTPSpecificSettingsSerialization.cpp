// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "HTTPSpecificSettings.hpp"

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <QJsonObject>
#include <QJsonValue>
#include <QString>

template <>
void DataStreamReader::read(const Engine::Network::HTTPSpecificSettings& n)
{
  m_stream << n.text;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Engine::Network::HTTPSpecificSettings& n)
{
  m_stream >> n.text;
  checkDelimiter();
}

template <>
void JSONObjectReader::read(const Engine::Network::HTTPSpecificSettings& n)
{
  obj["Text"] = n.text;
}

template <>
void JSONObjectWriter::write(Engine::Network::HTTPSpecificSettings& n)
{
  n.text = obj["Text"].toString();
}
