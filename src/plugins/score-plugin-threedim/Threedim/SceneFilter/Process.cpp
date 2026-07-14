#include "Process.hpp"

#include <score/application/ApplicationComponents.hpp>

#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>

#include <Gfx/Graph/Node.hpp>
#include <Gfx/TexturePort.hpp>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Gfx::SceneFilter::Model)
namespace Gfx::SceneFilter
{

Model::Model(
    const TimeVal& duration, const Id<Process::ProcessModel>& id, QObject* parent)
    : Process::ProcessModel{duration, id, "gfxProcess", parent}
{
  metadata().setInstanceName(*this);
  init();
}

Model::~Model() = default;

void Model::init()
{
  if(m_inlets.empty() && m_outlets.empty())
  {
    m_inlets.push_back(new GeometryInlet{"Scene In", Id<Process::Port>(0), this});
    m_inlets.push_back(new Process::ComboBox{
        std::vector<std::pair<QString, ossia::value>>{
            {QStringLiteral("pass through"),     0},
            {QStringLiteral("keep visible only"),1}},
        0, "Mode", Id<Process::Port>(1), this});
    m_outlets.push_back(new GeometryOutlet{"Scene Out", Id<Process::Port>(0), this});
  }
}

QString Model::prettyName() const noexcept
{
  return tr("Scene Filter");
}

}

template <>
void DataStreamReader::read(const Gfx::SceneFilter::Model& proc)
{
  readPorts(*this, proc.m_inlets, proc.m_outlets);
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Gfx::SceneFilter::Model& proc)
{
  writePorts(
      *this, components.interfaces<Process::PortFactoryList>(), proc.m_inlets,
      proc.m_outlets, &proc);
  checkDelimiter();
}

template <>
void JSONReader::read(const Gfx::SceneFilter::Model& proc)
{
  readPorts(*this, proc.m_inlets, proc.m_outlets);
}

template <>
void JSONWriter::write(Gfx::SceneFilter::Model& proc)
{
  writePorts(
      *this, components.interfaces<Process::PortFactoryList>(), proc.m_inlets,
      proc.m_outlets, &proc);
}
