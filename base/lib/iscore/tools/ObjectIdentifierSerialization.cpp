#include "iscore/serialization/DataStreamVisitor.hpp"
#include "iscore/serialization/JSONVisitor.hpp"
#include "ObjectIdentifier.hpp"

template<>
void Visitor<Reader<DataStream>>::readFrom(const ObjectIdentifier& obj)
{
    m_stream << obj.objectName();
    readFrom(obj.id());
}

template<>
void Visitor<Writer<DataStream>>::writeTo(ObjectIdentifier& obj)
{
    QString name;
    boost::optional<int32_t> id;
    m_stream >> name;
    writeTo(id);
    obj = ObjectIdentifier{name, id};
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const ObjectIdentifier& obj)
{
    m_obj["ObjectName"] = obj.objectName();
    m_obj["ObjectId"] = toJsonObject(obj.id());
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(ObjectIdentifier& obj)
{
    auto name = m_obj["ObjectName"].toString();

    boost::optional<int32_t> id;
    fromJsonObject(m_obj["ObjectId"].toObject(), id);

    obj = ObjectIdentifier{name, id};
}
