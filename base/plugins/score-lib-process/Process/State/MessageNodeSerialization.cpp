// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "MessageNode.hpp"

#include <State/Value.hpp>
#include <State/ValueSerialization.hpp>

#include <score/model/Identifier.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONValueVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>

#include <QDataStream>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QVector>
#include <QtGlobal>

#include <algorithm>
#include <array>
#include <cstddef>

namespace Process
{
class ProcessModel;
}
namespace boost
{
template <class T>
class optional;
} // namespace boost

template <typename T>
void toJsonValue(
    QJsonObject& object, const QString& name, const optional<T>& value)
{
  if (value)
  {
    object[name] = score::marshall<JSONObject>(*value);
  }
}

template <typename T, std::size_t N>
QJsonArray toJsonArray(const std::array<T, N>& array)
{
  QJsonArray arr;
  for (std::size_t i = 0; i < N; i++)
  {
    arr.append(toJsonValue(array.at(i)));
  }

  return arr;
}

template <typename T, std::size_t N>
void fromJsonArray(const QJsonArray& array, std::array<T, N>& res)
{
  for (std::size_t i = 0; i < N; i++)
  {
    res.at(i) = fromJsonValue<T>(array.at(i));
  }
}

template <typename T>
void fromJsonValue(
    const QJsonObject& object, const QString& name, optional<T>& value)
{
  auto it = object.find(name);
  if (it != object.end())
  {
    value = score::unmarshall<ossia::value>((*it).toObject());
  }
  else
  {
    value = ossia::none;
  }
}

template <>
void DataStreamReader::read(const Process::StateNodeValues& val)
{
  // TODO also serialize the index in the process
  // m_stream << val.userValue ;
}

template <>
void DataStreamWriter::write(Process::StateNodeValues& val)
{
  // m_stream >> val.userValue ;
}

template <>
void JSONObjectReader::read(const Process::StateNodeValues& val)
{
  //toJsonValue(obj, strings.User, val.userValue);
}

template <>
void JSONObjectWriter::write(Process::StateNodeValues& val)
{
  //fromJsonValue(obj, strings.User, val.userValue);
}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read(const Process::StateNodeData& node)
{
  m_stream << node.name << node.values;
  insertDelimiter();
}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write(Process::StateNodeData& node)
{
  m_stream >> node.name >> node.values;
  checkDelimiter();
}

template <>
SCORE_LIB_PROCESS_EXPORT void
JSONObjectReader::read(const Process::StateNodeData& node)
{
  obj[strings.Name] = node.name;
  readFrom(node.values);
}

template <>
SCORE_LIB_PROCESS_EXPORT void
JSONObjectWriter::write(Process::StateNodeData& node)
{
  node.name = obj[strings.Name].toString();
  writeTo(node.values);
}
