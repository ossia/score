#include <boost/optional/optional.hpp>
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
    boost::optional<int32_t> id;
    m_stream >> name;
    writeTo(id);
    obj = ObjectIdentifier{name, id};
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const ObjectIdentifier& obj)
{
    m_obj[iscore::StringConstant().ObjectName] = obj.objectName();
    m_obj[iscore::StringConstant().ObjectId] = toJsonObject(obj.id());
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(ObjectIdentifier& obj)
{
    auto name = m_obj[iscore::StringConstant().ObjectName].toString();

    boost::optional<int32_t> id;
    fromJsonObject(m_obj[iscore::StringConstant().ObjectId].toObject(), id);

    obj = ObjectIdentifier{name, id};
}
