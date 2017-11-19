// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <MidiUtil/MidiUtilProcess.hpp>
#include <Process/Dataflow/Port.hpp>

namespace MidiUtil
{
ProcessModel::ProcessModel(
    const TimeVal& duration,
    const Id<Process::ProcessModel>& id,
    QObject* parent)
  : Process::ProcessModel{duration, id,
                          Metadata<ObjectKey_k, ProcessModel>::get(), parent}
//  , outlet{std::make_unique<Process::Port>(Id<Process::Port>(0), this)}
{
  /*
  outlet->outlet = true;
  outlet->type = Process::PortType::Midi;
  */
  metadata().setInstanceName(*this);

}

ProcessModel::ProcessModel(
    const ProcessModel& source,
    const Id<Process::ProcessModel>& id,
    QObject* parent)
  : Process::ProcessModel{source, id,
                          Metadata<ObjectKey_k, ProcessModel>::get(), parent}
//  , outlet{std::make_unique<Process::Port>(source.outlet->id(), *source.outlet, this)}
{
  metadata().setInstanceName(*this);
}

ProcessModel::~ProcessModel()
{
}

Process::Inlets ProcessModel::inlets() const
{
  return m_inlets;
}

Process::Outlets ProcessModel::outlets() const
{
  return m_outlets;
}

}

template <>
void DataStreamReader::read(const MidiUtil::ProcessModel& proc)
{
//  m_stream << *proc.outlet;

  insertDelimiter();
}


template <>
void DataStreamWriter::write(MidiUtil::ProcessModel& proc)
{
//  proc.outlet = std::make_unique<Process::Port>(*this, &proc);
  checkDelimiter();
}


template <>
void JSONObjectReader::read(const MidiUtil::ProcessModel& proc)
{
//  obj["Outlet"] = toJsonObject(*proc.outlet);
}


template <>
void JSONObjectWriter::write(MidiUtil::ProcessModel& proc)
{
//  JSONObjectWriter writer{obj["Outlet"].toObject()};
//  proc.outlet = std::make_unique<Process::Port>(writer, &proc);
}
