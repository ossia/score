#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include "NamedObject.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

template <typename T> class Reader;
template <typename T> class Writer;

template<>
ISCORE_LIB_BASE_EXPORT void Visitor<Reader<DataStream>>::readFrom(const NamedObject& namedObject)
{
    m_stream << namedObject.objectName();
}

template<>
ISCORE_LIB_BASE_EXPORT void Visitor<Writer<DataStream>>::writeTo(NamedObject& namedObject)
{
    QString objectName;
    m_stream >> objectName;
    namedObject.setObjectName(objectName);
}


template<>
ISCORE_LIB_BASE_EXPORT void Visitor<Reader<JSONObject>>::readFrom(const NamedObject& namedObject)
{
    m_obj[iscore::StringConstant().ObjectName] = namedObject.objectName();
}

template<>
ISCORE_LIB_BASE_EXPORT void Visitor<Writer<JSONObject>>::writeTo(NamedObject& namedObject)
{
    namedObject.setObjectName(m_obj[iscore::StringConstant().ObjectName].toString());

}
