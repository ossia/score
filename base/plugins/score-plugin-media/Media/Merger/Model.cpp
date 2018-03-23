#include "Model.hpp"
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortFactory.hpp>

namespace Media
{
namespace Merger
{


Model::Model(
    const TimeVal& duration,
    const Id<Process::ProcessModel>& id,
    QObject* parent):
  Process::ProcessModel{duration, id, Metadata<ObjectKey_k, ProcessModel>::get(), parent}
{
  m_outlets.push_back(Process::make_outlet(Id<Process::Port>(std::numeric_limits<int16_t>::max()), this).release());
  m_outlets.back()->type = Process::PortType::Audio;
  setInCount(8);
  metadata().setInstanceName(*this);
}

Model::~Model()
{

}

quint64 Model::inCount() const { return m_inCount; }

void Model::setInCount(quint64 s)
{
  if(s != m_inCount)
  {
    auto old = m_inCount;
    m_inCount = s;

    if(old < m_inCount)
    {
      for(int i = 0; i < (m_inCount - old); i++)
      {
        m_inlets.push_back(Process::make_inlet(Id<Process::Port>(old + i), this).release());
        m_inlets.back()->type = Process::PortType::Audio;
      }
    }
    else if(old > m_inCount)
    {
      for(int i = m_inCount; i < old; i++)
      {
        delete m_inlets[i];
      }
      m_inlets.resize(m_inCount);
    }
    inletsChanged();

    inCountChanged(s);
  }
}

}
}
template <>
void DataStreamReader::read(const Media::Merger::Model& proc)
{
  readPorts(*this, proc.m_inlets, proc.m_outlets);
  m_stream << proc.m_inCount;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Media::Merger::Model& proc)
{
  writePorts(*this, components.interfaces<Process::PortFactoryList>(), proc.m_inlets, proc.m_outlets, &proc);

  m_stream >> proc.m_inCount;
  checkDelimiter();
}

template <>
void JSONObjectReader::read(const Media::Merger::Model& proc)
{
  readPorts(obj, proc.m_inlets, proc.m_outlets);
  obj["InCount"] = (qint64)proc.m_inCount;
}

template <>
void JSONObjectWriter::write(Media::Merger::Model& proc)
{
  writePorts(obj, components.interfaces<Process::PortFactoryList>(), proc.m_inlets, proc.m_outlets, &proc);
  proc.m_inCount = obj["InCount"].toInt();
}
