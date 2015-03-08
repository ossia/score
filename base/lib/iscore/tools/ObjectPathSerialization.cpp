#include "iscore/serialization/DataStreamVisitor.hpp"
#include "iscore/serialization/JSONVisitor.hpp"
#include "ObjectPath.hpp"

template<>
void Visitor<Reader<DataStream>>::readFrom(const ObjectPath& path)
{
    m_stream << path.m_objectIdentifiers;
}

template<>
void Visitor<Writer<DataStream>>::writeTo(ObjectPath& path)
{
    m_stream >> path.m_objectIdentifiers;
}

template<>
void Visitor<Reader<JSON>>::readFrom(const ObjectPath& path)
{
    m_obj["Identifiers"] = toJsonArray(path.m_objectIdentifiers);
}

template<>
void Visitor<Writer<JSON>>::writeTo(ObjectPath& path)
{
    fromJsonArray(m_obj["Identifiers"].toArray(), path.m_objectIdentifiers);
}
