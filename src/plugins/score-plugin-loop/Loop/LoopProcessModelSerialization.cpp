// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "LoopProcessModel.hpp"

#include <Process/Dataflow/Port.hpp>

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONValueVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

template <>
void DataStreamReader::read(const Loop::ProcessModel& proc)
{
  readFrom(static_cast<const Scenario::BaseScenarioContainer&>(proc));

  // Ports
  m_stream << *proc.inlet << *proc.outlet;

  insertDelimiter();
}

template <>
void DataStreamWriter::write(Loop::ProcessModel& proc)
{
  writeTo(static_cast<Scenario::BaseScenarioContainer&>(proc));

  // Ports
  proc.inlet = Process::load_audio_inlet(*this, &proc);
  proc.outlet = Process::load_audio_outlet(*this, &proc);

  checkDelimiter();
}

template <>
void JSONReader::read(const Loop::ProcessModel& proc)
{
  readFrom(static_cast<const Scenario::BaseScenarioContainer&>(proc));
  obj["Inlet"] = *proc.inlet;
  obj["Outlet"] = *proc.outlet;
}

template <>
void JSONWriter::write(Loop::ProcessModel& proc)
{
  writeTo(static_cast<Scenario::BaseScenarioContainer&>(proc));
  {
    JSONWriter writer{obj["Inlet"]};
    proc.inlet = Process::load_audio_inlet(writer, &proc);
  }
  {
    JSONWriter writer{obj["Outlet"]};
    proc.outlet = Process::load_audio_outlet(writer, &proc);
  }
}
