#include "WiimoteSpecificSettings.hpp"

#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

template <>
void DataStreamReader::read(const Engine::Network::WiimoteSpecificSettings& n)
{
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Engine::Network::WiimoteSpecificSettings& n)
{
  checkDelimiter();
}

template <>
void JSONObjectReader::read(const Engine::Network::WiimoteSpecificSettings& n)
{
}

template <>
void JSONObjectWriter::write(Engine::Network::WiimoteSpecificSettings& n)
{
}
