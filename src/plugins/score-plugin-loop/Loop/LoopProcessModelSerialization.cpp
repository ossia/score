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
  proc.inlet = Process::load_inlet(*this, &proc);
  proc.outlet = Process::load_audio_outlet(*this, &proc);

  checkDelimiter();
}

template <>
void JSONObjectReader::read(const Loop::ProcessModel& proc)
{
  readFrom(static_cast<const Scenario::BaseScenarioContainer&>(proc));
  obj["Inlet"] = toJsonObject(*proc.inlet);
  obj["Outlet"] = toJsonObject(*proc.outlet);
}

template <>
void JSONObjectWriter::write(Loop::ProcessModel& proc)
{
  writeTo(static_cast<Scenario::BaseScenarioContainer&>(proc));
  {
    JSONObjectWriter writer{obj["Inlet"].toObject()};
    proc.inlet = Process::load_inlet(writer, &proc);
    if (!proc.inlet)
    {
      proc.inlet = Process::make_audio_inlet(Id<Process::Port>(0), &proc);
      proc.inlet->type = Process::PortType::Audio;
    }
  }
  {
    JSONObjectWriter writer{obj["Outlet"].toObject()};
    proc.outlet = Process::load_audio_outlet(writer, &proc);

    if (!proc.outlet)
    {
      proc.outlet = Process::make_audio_outlet(Id<Process::Port>(0), &proc);
      proc.outlet->type = Process::PortType::Audio;
    }
  }
}
