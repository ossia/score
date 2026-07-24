#include "Process.hpp"

#include <score/application/ApplicationComponents.hpp>

#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>

#include <Gfx/Graph/Node.hpp>
#include <Gfx/TexturePort.hpp>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Gfx::ScenePreprocessor::Model)
namespace Gfx::ScenePreprocessor
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
    // Single Geometry Out — all material-texture arrays (base_color,
    // metal_rough, normal, emissive), camera / env / scene UBOs and the
    // environment skybox ride along as auxiliary_buffer / auxiliary_texture
    // entries on the emitted geometry. Consumer shaders auto-resolve them
    // by name via try_bind_from_geometry / try_bind_texture_from_geometry;
    // no manual cable needed.
    m_outlets.push_back(new GeometryOutlet{"Geometry Out", Id<Process::Port>(0), this});
  }
}

QString Model::prettyName() const noexcept
{
  return tr("Scene Preprocessor");
}

}

template <>
void DataStreamReader::read(const Gfx::ScenePreprocessor::Model& proc)
{
  readPorts(*this, proc.m_inlets, proc.m_outlets);
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Gfx::ScenePreprocessor::Model& proc)
{
  writePorts(
      *this,
      components.interfaces<Process::PortFactoryList>(),
      proc.m_inlets,
      proc.m_outlets,
      &proc);
  checkDelimiter();
}

template <>
void JSONReader::read(const Gfx::ScenePreprocessor::Model& proc)
{
  readPorts(*this, proc.m_inlets, proc.m_outlets);
}

template <>
void JSONWriter::write(Gfx::ScenePreprocessor::Model& proc)
{
  writePorts(
      *this,
      components.interfaces<Process::PortFactoryList>(),
      proc.m_inlets,
      proc.m_outlets,
      &proc);
}
