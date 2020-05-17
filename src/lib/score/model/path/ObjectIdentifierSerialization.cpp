// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ObjectIdentifier.hpp"

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/tools/std/Optional.hpp>

#include <QString>

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
void JSONReader::read(const ObjectIdentifier& id)
{
  stream.StartObject();
  obj[strings.ObjectName] = id.objectName();
  obj[strings.ObjectId] = id.id();
  stream.EndObject();
}

template <>
void JSONWriter::write(ObjectIdentifier& id)
{
  id = ObjectIdentifier{obj[strings.ObjectName].toString(), obj[strings.ObjectId].toInt()};
}
