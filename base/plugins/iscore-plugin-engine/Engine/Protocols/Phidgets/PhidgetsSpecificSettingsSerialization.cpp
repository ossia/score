#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

#include "PhidgetsSpecificSettings.hpp"


template <>
void DataStreamReader::read(
    const Engine::Network::PhidgetSpecificSettings& n)
{
  insertDelimiter();
}


template <>
void DataStreamWriter::write(
    Engine::Network::PhidgetSpecificSettings& n)
{
  checkDelimiter();
}


template <>
void JSONObjectReader::read(
    const Engine::Network::PhidgetSpecificSettings& n)
{

}


template <>
void JSONObjectWriter::write(
    Engine::Network::PhidgetSpecificSettings& n)
{
}
