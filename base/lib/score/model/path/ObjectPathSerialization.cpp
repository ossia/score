// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ObjectPath.hpp"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

template <>
SCORE_LIB_BASE_EXPORT void DataStreamReader::read(const ObjectPath& path)
{
  readFrom(path.vec());
}

template <>
SCORE_LIB_BASE_EXPORT void DataStreamWriter::write(ObjectPath& path)
{
  writeTo(path.vec());
}

template <>
SCORE_LIB_BASE_EXPORT void JSONObjectReader::read(const ObjectPath& path)
{
  obj[strings.Identifiers] = toJsonArray(path.vec());
}

template <>
SCORE_LIB_BASE_EXPORT void JSONObjectWriter::write(ObjectPath& path)
{
  fromJsonArray(obj[strings.Identifiers].toArray(), path.vec());
}
