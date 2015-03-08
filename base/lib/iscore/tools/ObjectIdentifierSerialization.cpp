#include "iscore/serialization/DataStreamVisitor.hpp"
#include "iscore/serialization/JSONVisitor.hpp"
#include "ObjectIdentifier.hpp"

template<>
void Visitor<Reader<DataStream>>::readFrom(const ObjectIdentifier& obj)
{
    m_stream << obj.m_objectName;
    readFrom(obj.m_id);
}

template<>
void Visitor<Writer<DataStream>>::writeTo(ObjectIdentifier& obj)
{
    m_stream >> obj.m_objectName;
    writeTo(obj.m_id);
}

template<>
void Visitor<Reader<JSON>>::readFrom(const ObjectIdentifier& obj)
{
    m_obj["ObjectName"] = obj.m_objectName;
    m_obj["ObjectId"] = toJsonObject(obj.m_id);
}

template<>
void Visitor<Writer<JSON>>::writeTo(ObjectIdentifier& obj)
{
    obj.m_objectName = m_obj["ObjectName"].toString();
    fromJsonObject(m_obj["ObjectId"].toObject(), obj.m_id);
}
