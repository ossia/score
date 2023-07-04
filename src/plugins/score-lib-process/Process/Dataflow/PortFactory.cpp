#include <Process/Dataflow/PortFactory.hpp>

#include <score/model/EntitySerialization.hpp>
#include <score/plugins/SerializableHelpers.hpp>

namespace Process
{
void readPorts(
    DataStreamReader& wr, const Process::Inlets& ins, const Process::Outlets& outs)
{
  wr.m_stream << static_cast<const ossia::small_vector<Process::Inlet*, 4>&>(ins);
  wr.m_stream << static_cast<const ossia::small_vector<Process::Outlet*, 4>&>(outs);
}

void writePorts(
    DataStreamWriter& wr, const Process::PortFactoryList& pl, Process::Inlets& ins,
    Process::Outlets& outs, QObject* parent)
{
  qDeleteAll(ins);
  qDeleteAll(outs);
  ins.clear();
  outs.clear();

  ArrayEntitySerializer::writeTo(
      wr, pl, parent,
      [&](auto* port) { ins.push_back(safe_cast<Process::Inlet*>(port)); },
      [&] { SCORE_ABORT; });

  ArrayEntitySerializer::writeTo(
      wr, pl, parent,
      [&](auto* port) { outs.push_back(safe_cast<Process::Outlet*>(port)); },
      [&] { SCORE_ABORT; });
}
void readPorts(JSONReader& obj, const Process::Inlets& ins, const Process::Outlets& outs)
{
  obj.obj["Inlets"] = static_cast<const ossia::small_vector<Process::Inlet*, 4>&>(ins);
  obj.obj["Outlets"]
      = static_cast<const ossia::small_vector<Process::Outlet*, 4>&>(outs);
}
void writePorts(
    const JSONWriter& obj, const Process::PortFactoryList& pl, Process::Inlets& ins,
    Process::Outlets& outs, QObject* parent)
{
  qDeleteAll(ins);
  qDeleteAll(outs);
  ins.clear();
  outs.clear();

  ArrayEntitySerializer::writeTo(
      JSONWriter{obj.obj["Inlets"]}, pl, parent,
      [&](auto* port) { ins.push_back(safe_cast<Process::Inlet*>(port)); },
      [&](const auto&) { SCORE_ABORT; });

  ArrayEntitySerializer::writeTo(
      JSONWriter{obj.obj["Outlets"]}, pl, parent,
      [&](auto* port) { outs.push_back(safe_cast<Process::Outlet*>(port)); },
      [&](const auto&) { SCORE_ABORT; });
}
}
