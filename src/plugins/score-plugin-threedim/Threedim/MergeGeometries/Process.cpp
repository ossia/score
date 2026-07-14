#include "Process.hpp"

#include <score/application/ApplicationComponents.hpp>

#include <Process/Dataflow/Port.hpp>

#include <Gfx/Graph/Node.hpp>
#include <Gfx/TexturePort.hpp>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Gfx::MergeGeometries::Model)
namespace Gfx::MergeGeometries
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
    for(int i = 0; i < 8; ++i)
    {
      QString name = QStringLiteral("Geometry %1").arg(i + 1);
      m_inlets.push_back(new GeometryInlet{name, Id<Process::Port>(i), this});
    }
    m_outlets.push_back(new GeometryOutlet{"Merged", Id<Process::Port>(0), this});
  }
}

QString Model::prettyName() const noexcept
{
  return tr("Merge Geometries");
}

}

template <>
void DataStreamReader::read(const Gfx::MergeGeometries::Model& proc)
{
  readPorts(*this, proc.m_inlets, proc.m_outlets);
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Gfx::MergeGeometries::Model& proc)
{
  writePorts(
      *this, components.interfaces<Process::PortFactoryList>(), proc.m_inlets,
      proc.m_outlets, &proc);
  checkDelimiter();
}

template <>
void JSONReader::read(const Gfx::MergeGeometries::Model& proc)
{
  readPorts(*this, proc.m_inlets, proc.m_outlets);
}

template <>
void JSONWriter::write(Gfx::MergeGeometries::Model& proc)
{
  writePorts(
      *this, components.interfaces<Process::PortFactoryList>(), proc.m_inlets,
      proc.m_outlets, &proc);
}
