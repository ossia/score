#include "DataflowProcess.hpp"
#include <iscore/model/path/PathSerialization.hpp>
namespace Process
{
ISCORE_LIB_PROCESS_EXPORT 
bool operator==(const CableData& lhs, const CableData& rhs)
{
  return lhs.type == rhs.type
      && lhs.source == rhs.source
      && lhs.sink == rhs.sink
      && lhs.outlet == rhs.outlet
      && lhs.inlet == rhs.inlet;
}

DataflowProcess::DataflowProcess(
        const DataflowProcess& source,
        const Id<Process::ProcessModel>& id,
        const QString& name,
        QObject* parent):
  Process::ProcessModel{source, id, name, parent}
, m_inlets{source.m_inlets}
, m_outlets{source.m_outlets}
{
  updateCounts();
}

DataflowProcess::~DataflowProcess()
{
  emit identified_object_destroying(this);
}

void DataflowProcess::setInlets(const std::vector<Port>& inlets)
{
  if(inlets != m_inlets)
  {
    m_inlets = inlets;
    updateCounts();
    emit inletsChanged();
  }
}

void DataflowProcess::setOutlets(const std::vector<Port>& outlets)
{
  if(outlets != m_outlets)
  {
    m_outlets = outlets;
    updateCounts();
    emit outletsChanged();
  }
}

const std::vector<Port>& DataflowProcess::inlets() const { return m_inlets; }

const std::vector<Port>& DataflowProcess::outlets() const { return m_outlets; }

void DataflowProcess::updateCounts()
{
  m_portCount = {};
  for(auto& p : m_inlets)
  {
    switch(p.type)
    {
      case PortType::Midi:
        m_portCount.midiIn++; break;
      case PortType::Audio:
        m_portCount.audioIn++; break;
      case PortType::Message:
        m_portCount.messageIn++; break;
    }
  }

  for(auto& p : m_outlets)
  {
    switch(p.type)
    {
      case PortType::Midi:
        m_portCount.midiOut++; break;
      case PortType::Audio:
        m_portCount.audioOut++; break;
      case PortType::Message:
        m_portCount.messageOut++; break;
    }
  }
}


}

template<>
void DataStreamReader::read<Process::Port>(const Process::Port& p)
{
  m_stream << p.type << p.customData << p.address;
}
template<>
void DataStreamWriter::write<Process::Port>(Process::Port& p)
{
  m_stream >> p.type >> p.customData >> p.address;
}

template<>
void DataStreamReader::read<Process::CableData>(const Process::CableData& p)
{
  m_stream << p.type << p.source << p.sink << p.outlet << p.inlet;
}
template<>
void DataStreamWriter::write<Process::CableData>(Process::CableData& p)
{
  m_stream >> p.type >> p.source >> p.sink >> p.outlet >> p.inlet;
}

template<>
void DataStreamReader::read<Process::Cable>(const Process::Cable& p)
{
  m_stream << (const Process::CableData&)p;
}
template<>
void DataStreamWriter::write<Process::Cable>(Process::Cable& p)
{
  m_stream >> (Process::CableData&)p;
}
