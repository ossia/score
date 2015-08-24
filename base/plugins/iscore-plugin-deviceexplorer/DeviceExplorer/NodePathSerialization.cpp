#include <iscore/tools/IdentifiedObject.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include "Node/Node.hpp"
template<>
void Visitor<Reader<DataStream>>::readFrom(const iscore::NodePath& path)
{
    m_stream << static_cast<const QList<int>&>(path);
}

template<>
void Visitor<Writer<DataStream>>::writeTo(iscore::NodePath& path)
{
    m_stream >> static_cast<QList<int>&>(path);
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const iscore::NodePath& path)
{
    m_obj["Path"] = toJsonArray(static_cast<const QList<int>&>(path));
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(iscore::NodePath& path)
{
    fromJsonArray(m_obj["Path"].toArray(), static_cast<QList<int>&>(path));
}
