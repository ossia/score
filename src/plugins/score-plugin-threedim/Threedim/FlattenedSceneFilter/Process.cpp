#include "Process.hpp"

#include <score/application/ApplicationComponents.hpp>

#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>

#include <Gfx/Graph/Node.hpp>
#include <Gfx/TexturePort.hpp>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Gfx::FlattenedSceneFilter::Model)
namespace Gfx::FlattenedSceneFilter
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
    m_inlets.push_back(new GeometryInlet{"Geometry In", Id<Process::Port>(0), this});

    m_inlets.push_back(new Process::ComboBox{
        std::vector<std::pair<QString, ossia::value>>{
            {QStringLiteral("tag == match"),                 0},
            {QStringLiteral("tag != match"),                 1},
            {QStringLiteral("material_index == match"),      2},
            {QStringLiteral("material_index != match"),      3},
            {QStringLiteral("blend == match"),               4},
            {QStringLiteral("blend != match"),               5},
            {QStringLiteral("depth_write == match"),         6},
            {QStringLiteral("depth_write != match"),         7},
            {QStringLiteral("cull_mode == match"),           8},
            {QStringLiteral("cull_mode != match"),           9},
            {QStringLiteral("topology == match"),            10},
            {QStringLiteral("topology != match"),            11},
            {QStringLiteral("format_id == match_str"),       12},
            {QStringLiteral("format_id != match_str"),       13}},
        0, "Mode", Id<Process::Port>(1), this});

    m_inlets.push_back(new Process::IntSpinBox{
        -1, 2147483647, 0, "Match", Id<Process::Port>(2), this});

    // Modes 12/13 read this string; other modes ignore it.
    m_inlets.push_back(new Process::LineEdit{
        QString{}, "Format ID", Id<Process::Port>(3), this});

    m_outlets.push_back(new GeometryOutlet{"Geometry Out", Id<Process::Port>(0), this});
  }
}

QString Model::prettyName() const noexcept
{
  return tr("Flattened Scene Filter");
}

}

template <>
void DataStreamReader::read(const Gfx::FlattenedSceneFilter::Model& proc)
{
  readPorts(*this, proc.m_inlets, proc.m_outlets);
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Gfx::FlattenedSceneFilter::Model& proc)
{
  writePorts(
      *this, components.interfaces<Process::PortFactoryList>(), proc.m_inlets,
      proc.m_outlets, &proc);
  checkDelimiter();
}

template <>
void JSONReader::read(const Gfx::FlattenedSceneFilter::Model& proc)
{
  readPorts(*this, proc.m_inlets, proc.m_outlets);
}

template <>
void JSONWriter::write(Gfx::FlattenedSceneFilter::Model& proc)
{
  writePorts(
      *this, components.interfaces<Process::PortFactoryList>(), proc.m_inlets,
      proc.m_outlets, &proc);
}
