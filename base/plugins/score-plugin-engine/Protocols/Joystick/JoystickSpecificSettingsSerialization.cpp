²
#include "JoystickSpecificSettings.hpp"

#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

template <>
void DataStreamReader::read(const Engine::Network::JoystickSpecificSettings& n)
{
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Engine::Network::JoystickSpecificSettings& n)
{
  checkDelimiter();
}

template <>
void JSONObjectReader::read(const Engine::Network::JoystickSpecificSettings& n)
{
}

template <>
void JSONObjectWriter::write(Engine::Network::JoystickSpecificSettings& n)
{
}
