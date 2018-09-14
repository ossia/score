// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "OSCQuerySpecificSettings.hpp"

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <QJsonObject>
#include <QJsonValue>
#include <QString>

template <>
void DataStreamReader::read(const Engine::Network::OSCQuerySpecificSettings& n)
{
  m_stream << n.host;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Engine::Network::OSCQuerySpecificSettings& n)
{
  m_stream >> n.host;
  checkDelimiter();
}

template <>
void JSONObjectReader::read(const Engine::Network::OSCQuerySpecificSettings& n)
{
  obj["Host"] = n.host;
}

template <>
void JSONObjectWriter::write(Engine::Network::OSCQuerySpecificSettings& n)
{
  n.host = obj["Host"].toString();
}
