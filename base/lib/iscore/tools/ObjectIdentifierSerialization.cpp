#include <iscore/tools/std/Optional.hpp>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <sys/types.h>

#include "ObjectIdentifier.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

template <typename T> class Reader;
template <typename T> class Writer;

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
    optional<int32_t> id;
    m_stream >> name;
    writeTo(id);
    obj = ObjectIdentifier{name, id};
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const ObjectIdentifier& obj)
{
    m_obj[strings.ObjectName] = obj.objectName();
    m_obj[strings.ObjectId] = toJsonObject(obj.id());
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(ObjectIdentifier& obj)
{
    auto name = m_obj[strings.ObjectName].toString();

    optional<int32_t> id;
    fromJsonObject(m_obj[strings.ObjectId], id);

    obj = ObjectIdentifier{name, id};
}
