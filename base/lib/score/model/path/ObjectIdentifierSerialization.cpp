// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ObjectIdentifier.hpp"

#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/tools/std/Optional.hpp>
#include <sys/types.h>

template <>
void DataStreamReader::read(const ObjectIdentifier& obj)
{
  m_stream << obj.objectName() << obj.id();
}

template <>
void DataStreamWriter::write(ObjectIdentifier& obj)
{
  QString name;
  int32_t id;
  m_stream >> name >> id;
  obj = ObjectIdentifier{name, id};
}

template <>
void JSONObjectReader::read(const ObjectIdentifier& id)
{
  obj[strings.ObjectName] = id.objectName();
  obj[strings.ObjectId] = id.id();
}

template <>
void JSONObjectWriter::write(ObjectIdentifier& id)
{
  id = ObjectIdentifier{obj[strings.ObjectName].toString(),
                        obj[strings.ObjectId].toInt()};
}
