#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <iscore/tools/std/Optional.hpp>
#include <sys/types.h>

#include "ObjectIdentifier.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

template <>
void Visitor<Reader<DataStream>>::readFrom(const ObjectIdentifier& obj)
{
  m_stream << obj.objectName() << obj.id();
}

template <>
void Visitor<Writer<DataStream>>::writeTo(ObjectIdentifier& obj)
{
  QString name;
  int32_t id;
  m_stream >> name >> id;
  obj = ObjectIdentifier{name, id};
}

template <>
void Visitor<Reader<JSONObject>>::readFrom(const ObjectIdentifier& obj)
{
  m_obj[strings.ObjectName] = obj.objectName();
  m_obj[strings.ObjectId] = obj.id();
}

template <>
void Visitor<Writer<JSONObject>>::writeTo(ObjectIdentifier& obj)
{
  obj = ObjectIdentifier{m_obj[strings.ObjectName].toString(),
                         m_obj[strings.ObjectId].toInt()};
}
