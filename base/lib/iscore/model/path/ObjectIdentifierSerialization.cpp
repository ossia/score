#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <iscore/tools/std/Optional.hpp>
#include <sys/types.h>

#include "ObjectIdentifier.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

template <>
void DataStreamReader::read(const ObjectIdentifier& obj)
{
  m_stream << obj.objectName() << obj.id();
}

template <>
void DataStreamWriter::writeTo(ObjectIdentifier& obj)
{
  QString name;
  int32_t id;
  m_stream >> name >> id;
  obj = ObjectIdentifier{name, id};
}

template<>
void JSONObjectReader::readFrom(const ObjectIdentifier& id)
{
  obj[strings.ObjectName] = id.objectName();
  obj[strings.ObjectId] = id.id();
}

template<>
void JSONObjectWriter::writeTo(ObjectIdentifier& id)
{
  id = ObjectIdentifier{obj[strings.ObjectName].toString(),
                         obj[strings.ObjectId].toInt()};
}
