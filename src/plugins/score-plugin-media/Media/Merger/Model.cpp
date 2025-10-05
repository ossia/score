#include "Model.hpp"

#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortFactory.hpp>

#include <score/application/ApplicationComponents.hpp>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Media::Merger::Model)
namespace Media
{
namespace Merger
{

Model::Model(
    const TimeVal& duration, const Id<Process::ProcessModel>& id, QObject* parent)
    : Process::ProcessModel{
        duration, id, Metadata<ObjectKey_k, ProcessModel>::get(), parent}
{
  auto out = std::make_unique<Process::AudioOutlet>(
      "Audio Out", Id<Process::Port>(std::numeric_limits<int16_t>::max()), this);
  out->setPropagate(true);
  m_outlets.push_back(out.release());

  setInCount(8);
  metadata().setInstanceName(*this);
}

Model::~Model() { }

auto Model::mode() const noexcept -> Mode
{
  return m_mode;
}

void Model::setMode(Mode m)
{
  if(m_mode == m)
    return;
  m_mode = m;
  modeChanged(m);
}

int Model::inCount() const noexcept
{
  return m_inCount;
}

void Model::setInCount(int s)
{
  if(s != m_inCount)
  {
    int old = m_inCount;
    m_inCount = s;

    if(old < m_inCount)
    {
      for(int i = 0; i < (m_inCount - old); i++)
      {
        m_inlets.push_back(
            std::make_unique<Process::AudioInlet>(
                "Audio In", Id<Process::Port>(int(old + i)), this)
                .release());
      }
      inletsChanged();
    }
    else if(old > m_inCount)
    {
      // Save the inlets
      ossia::small_pod_vector<Process::Inlet*, 8> old_ins;
      for(int i = m_inCount; i < old; i++)
        old_ins.push_back(m_inlets[i]);

      // Remove them from the known inlets of this process, and notify
      // (to delete views, etc. which must go before the data model)
      m_inlets.resize(m_inCount);

      inletsChanged();

      // Finally delete the inlet models
      for(auto* inl : old_ins)
        delete inl;
    }
    inCountChanged(s);
  }
}
}
}
template <>
void DataStreamReader::read(const Media::Merger::Model& proc)
{
  readPorts(*this, proc.m_inlets, proc.m_outlets);
  m_stream << proc.m_inCount << proc.m_mode;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Media::Merger::Model& proc)
{
  writePorts(
      *this, components.interfaces<Process::PortFactoryList>(), proc.m_inlets,
      proc.m_outlets, &proc);

  m_stream >> proc.m_inCount >> proc.m_mode;
  checkDelimiter();
}

template <>
void JSONReader::read(const Media::Merger::Model& proc)
{
  readPorts(*this, proc.m_inlets, proc.m_outlets);
  obj["InCount"] = proc.m_inCount;
  obj["Mode"] = proc.m_mode;
}

template <>
void JSONWriter::write(Media::Merger::Model& proc)
{
  writePorts(
      *this, components.interfaces<Process::PortFactoryList>(), proc.m_inlets,
      proc.m_outlets, &proc);
  proc.m_inCount = obj["InCount"].toInt();
  if(auto mode = obj.tryGet("Mode"))
  {
    proc.m_mode = (Media::Merger::Model::Mode)mode->toInt();
  }
  else
  {
    proc.m_mode = {};
  }
}
