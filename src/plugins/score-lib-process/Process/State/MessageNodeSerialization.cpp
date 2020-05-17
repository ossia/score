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

#include <QString>

#include <array>
#include <cstddef>

template <>
void DataStreamReader::read(const Process::ProcessStateData& val)
{
  m_stream << val.process << val.value;
}

template <>
void DataStreamWriter::write(Process::ProcessStateData& val)
{
  m_stream >> val.process >> val.value;
}

template <>
void JSONReader::read(const Process::ProcessStateData& val)
{
  stream.StartObject();
  obj[strings.Process] = val.process;
  obj[strings.Value] = val.value;
  stream.EndObject();
}

template <>
void JSONWriter::write(Process::ProcessStateData& val)
{
  val.process <<= obj[strings.Process];
  val.value <<= obj[strings.Value];
}

template <>
void DataStreamReader::read(const Process::StateNodeValues& val)
{
  m_stream << val.previousProcessValues << val.followingProcessValues << val.userValue
           << val.priorities;
}

template <>
void DataStreamWriter::write(Process::StateNodeValues& val)
{
  m_stream >> val.previousProcessValues >> val.followingProcessValues >> val.userValue
      >> val.priorities;
}

template <>
void JSONReader::read(const Process::StateNodeValues& val)
{
  obj[strings.Previous] = val.previousProcessValues;
  obj[strings.Following] = val.followingProcessValues;
  obj[strings.User] = val.userValue;
  obj[strings.Priorities] = val.priorities;
}

template <>
void JSONWriter::write(Process::StateNodeValues& val)
{
  val.previousProcessValues <<= obj[strings.Previous];
  val.followingProcessValues <<= obj[strings.Following];
  val.userValue <<= obj[strings.User];
  val.priorities <<= obj[strings.Priorities];
}

template <>
SCORE_LIB_PROCESS_EXPORT void DataStreamReader::read(const Process::StateNodeData& node)
{
  m_stream << node.name << node.values;
  insertDelimiter();
}

template <>
SCORE_LIB_PROCESS_EXPORT void DataStreamWriter::write(Process::StateNodeData& node)
{
  m_stream >> node.name >> node.values;
  checkDelimiter();
}

template <>
SCORE_LIB_PROCESS_EXPORT void JSONReader::read(const Process::StateNodeData& node)
{
  readFrom(node.name);
  readFrom(node.values);
}

template <>
SCORE_LIB_PROCESS_EXPORT void JSONWriter::write(Process::StateNodeData& node)
{
  writeTo(node.name);
  writeTo(node.values);
}
