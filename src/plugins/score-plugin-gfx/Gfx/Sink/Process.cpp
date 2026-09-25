#include "Process.hpp"

#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>

#include <Gfx/TexturePort.hpp>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Gfx::Sink::Model)
namespace Gfx::Sink
{
Model::Model(
    const TimeVal& duration, const Id<Process::ProcessModel>& id, QObject* parent)
    : Process::ProcessModel{duration, id, "gfxProcess", parent}
{
  metadata().setInstanceName(*this);
  m_inlets.push_back(new TextureInlet{tr("In"), Id<Process::Port>(0), this});

  auto rate = new Process::FloatSlider{tr("Rate"), Id<Process::Port>(1), this};
  rate->setDomain(ossia::make_domain(1.f, 120.f));
  rate->setValue(30.);
  rate->setDescription(tr(
      "Frames per second the connected processes run at; at most the render "
      "rate set in the settings"));
  m_inlets.push_back(rate);
}

Model::~Model() { }

QString Model::prettyName() const noexcept
{
  return tr("Sink");
}
}

template <>
void DataStreamReader::read(const Gfx::Sink::Model& proc)
{
  readPorts(*this, proc.m_inlets, proc.m_outlets);
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Gfx::Sink::Model& proc)
{
  writePorts(
      *this, components.interfaces<Process::PortFactoryList>(), proc.m_inlets,
      proc.m_outlets, &proc);
  checkDelimiter();
}

template <>
void JSONReader::read(const Gfx::Sink::Model& proc)
{
  readPorts(*this, proc.m_inlets, proc.m_outlets);
}

template <>
void JSONWriter::write(Gfx::Sink::Model& proc)
{
  writePorts(
      *this, components.interfaces<Process::PortFactoryList>(), proc.m_inlets,
      proc.m_outlets, &proc);
}
