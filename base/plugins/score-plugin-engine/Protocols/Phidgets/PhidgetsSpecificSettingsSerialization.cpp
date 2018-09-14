// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_PHIDGETS)
#include "PhidgetsSpecificSettings.hpp"

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <QJsonObject>
#include <QJsonValue>
#include <QString>

template <>
void DataStreamReader::read(const Engine::Network::PhidgetSpecificSettings& n)
{
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Engine::Network::PhidgetSpecificSettings& n)
{
  checkDelimiter();
}

template <>
void JSONObjectReader::read(const Engine::Network::PhidgetSpecificSettings& n)
{
}

template <>
void JSONObjectWriter::write(Engine::Network::PhidgetSpecificSettings& n)
{
}
#endif
