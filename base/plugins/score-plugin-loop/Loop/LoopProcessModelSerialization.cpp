// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QJsonObject>
#include <QJsonValue>
#include <Process/Dataflow/DataflowObjects.hpp>
#include "LoopProcessModel.hpp"
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONValueVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

template <typename T>
class Reader;
template <typename T>
class Writer;


template <>
void DataStreamReader::read(const Loop::ProcessModel& proc)
{
  readFrom(static_cast<const Scenario::BaseScenarioContainer&>(proc));

  // Ports
  m_stream << (int32_t)proc.ports().size();
  for (const auto& p : proc.ports())
  {
    readFrom(*p);
  }

  insertDelimiter();
}


template <>
void DataStreamWriter::write(Loop::ProcessModel& proc)
{
  writeTo(static_cast<Scenario::BaseScenarioContainer&>(proc));

  // Ports
  int32_t port_count;
  m_stream >> port_count;
  for (; port_count-- > 0;)
  {
    proc.m_ports.push_back(new Process::Port{*this, &proc});
  }

  checkDelimiter();
}


template <>
void JSONObjectReader::read(const Loop::ProcessModel& proc)
{
  readFrom(static_cast<const Scenario::BaseScenarioContainer&>(proc));
  obj["Ports"] = toJsonArray(proc.m_ports);
}


template <>
void JSONObjectWriter::write(Loop::ProcessModel& proc)
{
  writeTo(static_cast<Scenario::BaseScenarioContainer&>(proc));
  {
    QJsonArray port_array = obj["Ports"].toArray();
    for (const auto& json_vref : port_array)
    {
      JSONObject::Deserializer deserializer{json_vref.toObject()};
      proc.m_ports.push_back(new Process::Port{deserializer, &proc});
    }
  }
}
