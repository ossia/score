#include "Process.hpp"

#include <Gfx/Graph/Node.hpp>
#include <Gfx/TexturePort.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>
#include <wobjectimpl.h>

#include <QFileInfo>
#include <QImageReader>

W_OBJECT_IMPL(Gfx::ModelDisplay::Model)
namespace Gfx::ModelDisplay
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
    m_outlets.push_back(new TextureOutlet{Id<Process::Port>(0), this});
    m_inlets.push_back(new TextureInlet{Id<Process::Port>(0), this});
    m_inlets.push_back(new GeometryInlet{Id<Process::Port>(1), this});

    m_inlets.push_back(new Process::XYZSpinboxes{
        ossia::vec3f{-10000., -10000., -10000.}, ossia::vec3f{10000., 10000., 10000.},
        ossia::vec3f{-1., -1., -1.}, "Position", Id<Process::Port>(2), this});
    m_inlets.push_back(new Process::XYZSpinboxes{
        ossia::vec3f{-10000., -10000., -10000.}, ossia::vec3f{10000., 10000., 10000.},
        ossia::vec3f{}, "Center", Id<Process::Port>(3), this});

    m_inlets.push_back(
        new Process::FloatSlider{0.01, 359.999, 90., "FOV", Id<Process::Port>(4), this});
    m_inlets.push_back(new Process::FloatSlider{
        0.001, 1000., 0.001, "Near", Id<Process::Port>(5), this});
    m_inlets.push_back(new Process::FloatSlider{
        0.001, 10000., 100000., "Far", Id<Process::Port>(6), this});
  }
  if(m_inlets.size() <= 7)
  {
    std::vector<std::pair<QString, ossia::value>> projs{
        {"Texture coordinates", 0},
        {"Spherical", 2},
        {"View-space", 4},
        {"Barycentric", 5},
        {"Funky A", 1},
        {"Funky B", 3},
        {"Light", 6},
        {"Color", 7},
    };

    m_inlets.push_back(
        new Process::ComboBox{projs, 0, "Tex. Proj.", Id<Process::Port>(7), this});
  }

  if(m_inlets.size() <= 8)
  {
    std::vector<std::pair<QString, ossia::value>> modes{
        {"Triangles", 0},
        {"Points", 1},
        {"Lines", 2},
    };

    m_inlets.push_back(
        new Process::ComboBox{modes, 0, "Mode", Id<Process::Port>(8), this});
  }

  if(m_inlets.size() <= 9)
  {
    ((Process::FloatSlider*)(inlet(Id<Process::Port>(4))))
        ->setDomain(ossia::make_domain(0.01, 359.999));
    std::vector<std::pair<QString, ossia::value>> projmodes{
        {"Perspective", 0},
        {"Fulldome (1-pass)", 1},
    };

    m_inlets.push_back(
        new Process::ComboBox{projmodes, 0, "Camera", Id<Process::Port>(9), this});
  }
}

QString Model::prettyName() const noexcept
{
  return tr("Model Display");
}

}
template <>
void DataStreamReader::read(const Gfx::ModelDisplay::Model& proc)
{
  readPorts(*this, proc.m_inlets, proc.m_outlets);

  insertDelimiter();
}

template <>
void DataStreamWriter::write(Gfx::ModelDisplay::Model& proc)
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
void JSONReader::read(const Gfx::ModelDisplay::Model& proc)
{
  readPorts(*this, proc.m_inlets, proc.m_outlets);
}

template <>
void JSONWriter::write(Gfx::ModelDisplay::Model& proc)
{
  writePorts(
      *this,
      components.interfaces<Process::PortFactoryList>(),
      proc.m_inlets,
      proc.m_outlets,
      &proc);
}
