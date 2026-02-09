#include "Process.hpp"

#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>

#include <Gfx/Graph/Node.hpp>
#include <Gfx/TexturePort.hpp>

#include <QFileInfo>
#include <QImageReader>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Gfx::Splat::Model)
namespace Gfx::Splat
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
    m_outlets.push_back(new TextureOutlet{"Texture Out", Id<Process::Port>(0), this});
    m_inlets.push_back(new TextureInlet{"Buffer In", Id<Process::Port>(0), this});

    m_inlets.push_back(new Process::XYZSpinboxes{
        ossia::vec3f{-10000., -10000., -10000.}, ossia::vec3f{10000., 10000., 10000.},
        ossia::vec3f{0., 0., 0.}, false, "Position", Id<Process::Port>(1), this});
    m_inlets.push_back(new Process::XYZSpinboxes{
        ossia::vec3f{0., 0., 0.}, ossia::vec3f{359.9999999, 359.9999999, 359.9999999},
        ossia::vec3f{}, false, "Rotation", Id<Process::Port>(2), this});
    m_inlets.push_back(new Process::XYZSpinboxes{
        ossia::vec3f{0.00001, 0.00001, 0.00001}, ossia::vec3f{1000., 1000., 1000.},
        ossia::vec3f{1., 1., 1.}, false, "Scale", Id<Process::Port>(3), this});

    m_inlets.push_back(new Process::XYZSpinboxes{
        ossia::vec3f{-10000., -10000., -10000.}, ossia::vec3f{10000., 10000., 10000.},
        ossia::vec3f{-1., -1., -1.}, false, "Camera position", Id<Process::Port>(4),
        this});
    m_inlets.push_back(new Process::XYZSpinboxes{
        ossia::vec3f{-10000., -10000., -10000.}, ossia::vec3f{10000., 10000., 10000.},
        ossia::vec3f{}, false, "Camera direction", Id<Process::Port>(5), this});

    m_inlets.push_back(
        new Process::FloatSlider{0.01, 359.999, 90., "FOV", Id<Process::Port>(6), this});
    m_inlets.push_back(new Process::FloatSlider{
        0.001, 1000., 0.001, "Near", Id<Process::Port>(7), this});
    m_inlets.push_back(new Process::FloatSlider{
        0.001, 10000., 100000., "Far", Id<Process::Port>(8), this});
  }

  std::vector<std::pair<QString, ossia::value>> projmodes{
      {"Perspective", 0},
      {"Fulldome (1-pass)", 1},
  };

  m_inlets.push_back(
      new Process::ComboBox{projmodes, 0, "Camera", Id<Process::Port>(9), this});
}

QString Model::prettyName() const noexcept
{
  return tr("Model Display");
}

}
template <>
void DataStreamReader::read(const Gfx::Splat::Model& proc)
{
  readPorts(*this, proc.m_inlets, proc.m_outlets);

  insertDelimiter();
}

template <>
void DataStreamWriter::write(Gfx::Splat::Model& proc)
{
  writePorts(
      *this, components.interfaces<Process::PortFactoryList>(), proc.m_inlets,
      proc.m_outlets, &proc);

  checkDelimiter();
}

template <>
void JSONReader::read(const Gfx::Splat::Model& proc)
{
  readPorts(*this, proc.m_inlets, proc.m_outlets);
}

template <>
void JSONWriter::write(Gfx::Splat::Model& proc)
{
  writePorts(
      *this, components.interfaces<Process::PortFactoryList>(), proc.m_inlets,
      proc.m_outlets, &proc);
}
