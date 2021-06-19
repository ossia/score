#include "Process.hpp"

#include <Gfx/Graph/Node.hpp>
#include <Gfx/TexturePort.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>

#include <QFileInfo>
#include <QImageReader>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Gfx::Text::Model)
namespace Gfx::Text
{

Model::Model(
    const TimeVal& duration,
    const Id<Process::ProcessModel>& id,
    QObject* parent)
    : Process::ProcessModel{duration, id, "gfxProcess", parent}
{
  metadata().setInstanceName(*this);
  {
    auto text = new Process::LineEdit{tr("Greetings from Oscar !"), tr("Text"), Id<Process::Port>(0), this};
    m_inlets.push_back(text);
  }

  {
    auto font = new Process::LineEdit{tr("Monospace"), tr("Font"), Id<Process::Port>(1), this};
    m_inlets.push_back(font);
  }


  {
    auto pointSize = new Process::FloatSlider{Id<Process::Port>(2), this};
    pointSize->setName(tr("Point size"));
    pointSize->setValue(28.);
    pointSize->setDomain(ossia::make_domain(1.f, 300.f));
    m_inlets.push_back(pointSize);
  }

  {
    auto opacity = new Process::FloatSlider{Id<Process::Port>(3), this};
    opacity->setName(tr("Opacity"));
    opacity->setValue(1.);
    m_inlets.push_back(opacity);
  }

  {
    auto pos = new Process::XYSlider{Id<Process::Port>(4), this};
    pos->setName(tr("Position"));
    pos->setDomain(
      ossia::make_domain(ossia::vec2f{-5.0, -5.0}, ossia::vec2f{5.0, 5.0}));

    m_inlets.push_back(pos);
  }
  {
    auto scale = new Process::XYSlider{Id<Process::Port>(5), this};
    scale->setName(tr("Scale"));
    scale->setValue(ossia::vec2f{1., 1.});
    scale->setDomain(
        ossia::make_domain(ossia::vec2f{0.01, 0.01}, ossia::vec2f{10., 10.}));
    m_inlets.push_back(scale);
  }

  m_outlets.push_back(new TextureOutlet{Id<Process::Port>(0), this});
}

Model::~Model() { }

QString Model::prettyName() const noexcept
{
  return tr("Text");
}

}
template <>
void DataStreamReader::read(const Gfx::Text::Model& proc)
{
  readPorts(*this, proc.m_inlets, proc.m_outlets);

  insertDelimiter();
}

template <>
void DataStreamWriter::write(Gfx::Text::Model& proc)
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
void JSONReader::read(const Gfx::Text::Model& proc)
{
  readPorts(*this, proc.m_inlets, proc.m_outlets);
}

template <>
void JSONWriter::write(Gfx::Text::Model& proc)
{
  writePorts(
      *this,
      components.interfaces<Process::PortFactoryList>(),
      proc.m_inlets,
      proc.m_outlets,
      &proc);
}
