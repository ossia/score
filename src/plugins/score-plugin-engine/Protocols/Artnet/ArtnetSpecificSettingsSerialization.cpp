#include "ArtnetSpecificSettings.hpp"

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <QJsonObject>
#include <QJsonValue>
#include <QString>

template <>
void DataStreamReader::read(const Protocols::ArtnetSpecificSettings& n)
{
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Protocols::ArtnetSpecificSettings& n)
{
  checkDelimiter();
}

template <>
void JSONObjectReader::read(const Protocols::ArtnetSpecificSettings& n)
{
}

template <>
void JSONObjectWriter::write(Protocols::ArtnetSpecificSettings& n)
{
}
