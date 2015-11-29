#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include "NamedObject.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

template <typename T> class Reader;
template <typename T> class Writer;

template<>
void Visitor<Reader<DataStream>>::readFrom(const NamedObject& namedObject)
{
    m_stream << namedObject.objectName();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(NamedObject& namedObject)
{
    QString objectName;
    m_stream >> objectName;
    namedObject.setObjectName(objectName);
}


template<>
void Visitor<Reader<JSONObject>>::readFrom(const NamedObject& namedObject)
{
    m_obj["ObjectName"] = namedObject.objectName();
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(NamedObject& namedObject)
{
    namedObject.setObjectName(m_obj["ObjectName"].toString());

}
