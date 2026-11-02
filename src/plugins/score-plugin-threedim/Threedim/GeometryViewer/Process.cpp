#include "Process.hpp"

#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortSerialization.hpp>
#include <Process/Dataflow/PortFactory.hpp>

#include <score/plugins/SerializableHelpers.hpp>
#include <score/application/ApplicationComponents.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>

#include <Gfx/TexturePort.hpp>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Gfx::GeometryViewer::Model)
namespace Gfx::GeometryViewer
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
  if(!m_inlets.empty() || !m_outlets.empty())
    return;

  // Texture, then geometry: the order of the Model Display renderer
  m_inlets.push_back(new TextureInlet{"Texture In", Id<Process::Port>(8), this});
  m_inlets.push_back(new GeometryInlet{"Geometry In", Id<Process::Port>(0), this});
  m_inlets.push_back(new Process::ComboBox{
      std::vector<std::pair<QString, ossia::value>>{
          {"Shaded", int(Mode::Shaded)},
          {"Wireframe", int(Mode::Wireframe)},
          {"Points", int(Mode::Points)},
          {"Vertex colours", int(Mode::VertexColors)},
          {"Texture coordinates", int(Mode::TextureCoordinates)}},
      int(Mode::Shaded), "Mode", Id<Process::Port>(1), this});
  m_inlets.push_back(
      new Process::FloatSlider{-180., 180., 35., "Yaw", Id<Process::Port>(2), this});
  m_inlets.push_back(
      new Process::FloatSlider{-180., 180., 25., "Pitch", Id<Process::Port>(3), this});
  // 1 shows the whole geometry when it is fitted
  m_inlets.push_back(
      new Process::LogFloatSlider{0.01, 100., 1., "Zoom", Id<Process::Port>(4), this});
  // Added to the centre of the geometry when it is fitted
  m_inlets.push_back(new Process::XYZSpinboxes{
      ossia::vec3f{-1000., -1000., -1000.}, ossia::vec3f{1000., 1000., 1000.},
      ossia::vec3f{}, false, "Centre", Id<Process::Port>(5), this});
  m_inlets.push_back(
      new Process::FloatSlider{5., 170., 60., "Field of view", Id<Process::Port>(6), this});
  m_inlets.push_back(new Process::Toggle{false, "Auto-rotate", Id<Process::Port>(7), this});
  m_inlets.push_back(new Process::Toggle{true, "Auto-fit", Id<Process::Port>(9), this});
  m_inlets.push_back(new Process::ComboBox{
      std::vector<std::pair<QString, ossia::value>>{
          {"None", int(Background::None)},
          {"Black", int(Background::Black)},
          {"Dark", int(Background::Dark)},
          {"Grey", int(Background::Grey)},
          {"White", int(Background::White)},
          {"Gradient", int(Background::Gradient)}},
      int(Background::Gradient), "Background", Id<Process::Port>(10), this});
  m_inlets.push_back(new Process::Toggle{true, "Plane", Id<Process::Port>(11), this});
  m_inlets.push_back(new Process::Toggle{true, "Axes", Id<Process::Port>(12), this});
  // The lights and cameras of a scene
  m_inlets.push_back(new Process::Toggle{true, "Gizmos", Id<Process::Port>(13), this});

  m_outlets.push_back(new TextureOutlet{"Texture Out", Id<Process::Port>(0), this});
}

QString Model::prettyName() const noexcept
{
  return tr("Geometry viewer");
}
}

template <>
void DataStreamReader::read(const Gfx::GeometryViewer::Model& proc)
{
  readPorts(*this, proc.m_inlets, proc.m_outlets);
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Gfx::GeometryViewer::Model& proc)
{
  writePorts(
      *this, components.interfaces<Process::PortFactoryList>(), proc.m_inlets,
      proc.m_outlets, &proc);
  checkDelimiter();
}

template <>
void JSONReader::read(const Gfx::GeometryViewer::Model& proc)
{
  readPorts(*this, proc.m_inlets, proc.m_outlets);
}

template <>
void JSONWriter::write(Gfx::GeometryViewer::Model& proc)
{
  writePorts(
      *this, components.interfaces<Process::PortFactoryList>(), proc.m_inlets,
      proc.m_outlets, &proc);
}
