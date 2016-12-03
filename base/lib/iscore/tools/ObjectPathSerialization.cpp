#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

#include "ObjectPath.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

template <typename T>
class Reader;
template <typename T>
class Writer;

template <>
ISCORE_LIB_BASE_EXPORT void
Visitor<Reader<DataStream>>::readFrom(const ObjectPath& path)
{
  readFrom(path.vec());
}

template <>
ISCORE_LIB_BASE_EXPORT void
Visitor<Writer<DataStream>>::writeTo(ObjectPath& path)
{
  writeTo(path.vec());
}

template <>
ISCORE_LIB_BASE_EXPORT void
Visitor<Reader<JSONObject>>::readFrom(const ObjectPath& path)
{
  m_obj[strings.Identifiers] = toJsonArray(path.vec());
}

template <>
ISCORE_LIB_BASE_EXPORT void
Visitor<Writer<JSONObject>>::writeTo(ObjectPath& path)
{
  fromJsonArray(m_obj[strings.Identifiers].toArray(), path.vec());
}
