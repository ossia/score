#include <iscore/tools/std/StdlibWrapper.hpp>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

#include "ObjectPath.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

template <typename T> class Reader;
template <typename T> class Writer;

template<>
void Visitor<Reader<DataStream>>::readFrom(const ObjectPath& path)
{
    readFrom(path.vec());
}

template<>
void Visitor<Writer<DataStream>>::writeTo(ObjectPath& path)
{
    writeTo(path.vec());
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const ObjectPath& path)
{
    m_obj["Identifiers"] = toJsonArray(path.vec());
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(ObjectPath& path)
{
    fromJsonArray(m_obj["Identifiers"].toArray(), path.vec());
}
