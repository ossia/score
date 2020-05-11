#include "Process.hpp"

#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>

#include <QShaderBaker>

#include <Gfx/Graph/node.hpp>
#include <Gfx/Graph/nodes.hpp>
#include <Gfx/TexturePort.hpp>
#include <wobjectimpl.h>

W_OBJECT_IMPL(Gfx::Images::Model)
namespace Gfx::Images
{

Model::Model(
    const TimeVal& duration,
    const Id<Process::ProcessModel>& id,
    QObject* parent)
    : Process::ProcessModel{duration, id, "gfxProcess", parent}
{
  metadata().setInstanceName(*this);
  m_inlets.push_back(new Process::IntSpinBox{Id<Process::Port>(0), this});
  m_inlets.back()->setCustomData(tr("Index"));
  m_inlets.push_back(new Process::XYSlider{Id<Process::Port>(1), this});
  m_inlets.back()->setCustomData(tr("Position"));

  m_outlets.push_back(new TextureOutlet{Id<Process::Port>(0), this});

  m_images.push_back({"/home/jcelerier/Documents/ossia.png",
                      QImage("/home/jcelerier/Documents/ossia.png")});
  m_images.push_back({"/home/jcelerier/Documents/IMG_1929.JPG",
                      QImage("/home/jcelerier/Documents/IMG_1929.JPG")});
}

Model::~Model() {}

void Model::setImages(const std::vector<Image>& f)
{
  m_images = f;
  imagesChanged();
}

QString Model::prettyName() const noexcept
{
  return tr("Images");
}

void Model::startExecution() {}

void Model::stopExecution() {}

void Model::reset() {}

void Model::setDurationAndScale(const TimeVal& newDuration) noexcept {}

void Model::setDurationAndGrow(const TimeVal& newDuration) noexcept {}

void Model::setDurationAndShrink(const TimeVal& newDuration) noexcept {}

QSet<QString> DropHandler::mimeTypes() const noexcept
{
  return {}; // TODO
}

QSet<QString> LibraryHandler::acceptedFiles() const noexcept
{
  return {"png", "jpg", "jpeg", "gif", "bmp"};
}

QSet<QString> DropHandler::fileExtensions() const noexcept
{
  return {"png", "jpg", "jpeg", "gif", "bmp"};
}

std::vector<Process::ProcessDropHandler::ProcessDrop> DropHandler::dropData(
    const std::vector<DroppedFile>& data,
    const score::DocumentContext& ctx) const noexcept
{
  std::vector<Process::ProcessDropHandler::ProcessDrop> vec;
  {
    for (const auto& [filename, file] : data)
    {
      Process::ProcessDropHandler::ProcessDrop p;
      p.creation.key = Metadata<ConcreteKey_k, Gfx::Images::Model>::get();
      p.setup =
          [str = filename](Process::ProcessModel& m, score::Dispatcher& disp) {
            auto& midi = static_cast<Gfx::Images::Model&>(m);
            // TODO disp.submit(new ChangeImages{midi, str});
          };
      vec.push_back(std::move(p));
    }
  }
  return vec;
}

}
template <>
void DataStreamReader::read(const Gfx::Image& proc)
{
}

template <>
void DataStreamWriter::write(Gfx::Image& proc)
{
}

template <>
void JSONReader::read(const Gfx::Image& proc)
{
}

template <>
void JSONWriter::write(Gfx::Image& proc)
{
}

template <>
void DataStreamReader::read(const Gfx::Images::Model& proc)
{
  readPorts(*this, proc.m_inlets, proc.m_outlets);

  m_stream << proc.m_images;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Gfx::Images::Model& proc)
{
  writePorts(
      *this,
      components.interfaces<Process::PortFactoryList>(),
      proc.m_inlets,
      proc.m_outlets,
      &proc);

  m_stream >> proc.m_images;
  checkDelimiter();
}

template <>
void JSONReader::read(const Gfx::Images::Model& proc)
{
  readPorts(*this, proc.m_inlets, proc.m_outlets);
  // TODO
}

template <>
void JSONWriter::write(Gfx::Images::Model& proc)
{
  writePorts(
        *this,
      components.interfaces<Process::PortFactoryList>(),
      proc.m_inlets,
      proc.m_outlets,
      &proc);
  // TODO
}
