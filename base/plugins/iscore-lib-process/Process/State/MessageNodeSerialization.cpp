
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
#include <iscore/serialization/VisitorCommon.hpp>

#include "MessageNode.hpp"
#include <State/Value.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

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
class Reader;
template <typename T>
class Writer;

template <typename T>
void toJsonValue(
    QJsonObject& object, const QString& name, const optional<T>& value)
{
  if (value)
  {
    object[name] = marshall<JSONObject>(*value);
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
    value = unmarshall<State::Value>((*it).toObject());
  }
  else
  {
    value = iscore::none;
  }
}

template <>
void Visitor<Reader<DataStream>>::readFrom(
    const std::array<Process::PriorityPolicy, 3>& val)
{
  for (int i = 0; i < 3; i++)
    m_stream << val.at(i);
}

template <>
void Visitor<Writer<DataStream>>::writeTo(
    std::array<Process::PriorityPolicy, 3>& val)
{
  for (int i = 0; i < 3; i++)
    m_stream >> val.at(i);
}

template <>
void Visitor<Reader<DataStream>>::readFrom(
    const Process::ProcessStateData& val)
{
  m_stream << val.process << val.value;
}

template <>
void Visitor<Writer<DataStream>>::writeTo(Process::ProcessStateData& val)
{
  m_stream >> val.process >> val.value;
}

template <>
void Visitor<Reader<JSONObject>>::readFrom(
    const Process::ProcessStateData& val)
{
  m_obj[strings.Process] = toJsonValue(val.process);
  toJsonValue(m_obj, strings.Value, val.value);
}

template <>
void Visitor<Writer<JSONObject>>::writeTo(Process::ProcessStateData& val)
{
  val.process
      = fromJsonValue<Id<Process::ProcessModel>>(m_obj[strings.Process]);
  fromJsonValue(m_obj, strings.Value, val.value);
}

template <>
void Visitor<Reader<DataStream>>::readFrom(const Process::StateNodeValues& val)
{
  m_stream << val.previousProcessValues << val.followingProcessValues
           << val.userValue << val.priorities;
}

template <>
void Visitor<Writer<DataStream>>::writeTo(Process::StateNodeValues& val)
{
  m_stream >> val.previousProcessValues >> val.followingProcessValues
      >> val.userValue >> val.priorities;
}

template <>
void Visitor<Reader<JSONObject>>::readFrom(const Process::StateNodeValues& val)
{
  m_obj[strings.Previous] = toJsonArray(val.previousProcessValues);
  m_obj[strings.Following] = toJsonArray(val.followingProcessValues);
  toJsonValue(m_obj, strings.User, val.userValue);
  m_obj[strings.Priorities] = toJsonArray(val.priorities);
}

template <>
void Visitor<Writer<JSONObject>>::writeTo(Process::StateNodeValues& val)
{
  fromJsonArray(m_obj[strings.Previous].toArray(), val.previousProcessValues);
  fromJsonArray(
      m_obj[strings.Following].toArray(), val.followingProcessValues);
  fromJsonValue(m_obj, strings.User, val.userValue);
  fromJsonArray(m_obj[strings.Priorities].toArray(), val.priorities);
}

template <>
ISCORE_LIB_PROCESS_EXPORT void
Visitor<Reader<DataStream>>::readFrom(const Process::StateNodeData& node)
{
  m_stream << node.name << node.values;
  insertDelimiter();
}

template <>
ISCORE_LIB_PROCESS_EXPORT void
Visitor<Writer<DataStream>>::writeTo(Process::StateNodeData& node)
{
  m_stream >> node.name >> node.values;
  checkDelimiter();
}

template <>
ISCORE_LIB_PROCESS_EXPORT void
Visitor<Reader<JSONObject>>::readFrom(const Process::StateNodeData& node)
{
  readFrom(node.name);
  readFrom(node.values);
}

template <>
ISCORE_LIB_PROCESS_EXPORT void
Visitor<Writer<JSONObject>>::writeTo(Process::StateNodeData& node)
{
  writeTo(node.name);
  writeTo(node.values);
}
