#include <iscore/tools/IdentifiedObject.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include "NodePath.hpp"
template<>
void Visitor<Reader<DataStream>>::readFrom(const Path& path)
{
    m_stream << static_cast<const QList<int>&>(path);
}

template<>
void Visitor<Writer<DataStream>>::writeTo(Path& path)
{
    m_stream >> static_cast<QList<int>&>(path);
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const Path& path)
{
    m_obj["Path"] = toJsonArray(static_cast<const QList<int>&>(path));
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(Path& path)
{
    fromJsonArray(m_obj["Path"].toArray(), static_cast<QList<int>&>(path));
}
