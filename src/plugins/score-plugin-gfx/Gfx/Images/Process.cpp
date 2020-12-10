#include "Process.hpp"

#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>

#include <QFileInfo>

#include <Gfx/Graph/node.hpp>
#include <Gfx/Graph/nodes.hpp>
#include <Gfx/TexturePort.hpp>
#include <wobjectimpl.h>

W_OBJECT_IMPL(Gfx::Images::Model)
namespace Gfx::Images
{

Model::Model(const TimeVal& duration, const Id<Process::ProcessModel>& id, QObject* parent)
    : Process::ProcessModel{duration, id, "gfxProcess", parent}
{
  metadata().setInstanceName(*this);
  m_inlets.push_back(new Process::IntSpinBox{Id<Process::Port>(0), this});
  m_inlets.back()->setCustomData(tr("Index"));
  m_inlets.push_back(new Process::XYSlider{Id<Process::Port>(1), this});
  m_inlets.back()->setCustomData(tr("Position"));

  m_outlets.push_back(new TextureOutlet{Id<Process::Port>(0), this});

}

Model::~Model() { }

void Model::setImages(const std::vector<Image>& f)
{
  m_images = f;
  auto spinbox = safe_cast<Process::IntSpinBox*>(m_inlets[0]);
  spinbox->setDomain(ossia::make_domain(int(0), int(f.size())));
  imagesChanged();
}

QString Model::prettyName() const noexcept
{
  return tr("Images");
}

void Model::startExecution() { }

void Model::stopExecution() { }

void Model::reset() { }

void Model::setDurationAndScale(const TimeVal& newDuration) noexcept { }

void Model::setDurationAndGrow(const TimeVal& newDuration) noexcept { }

void Model::setDurationAndShrink(const TimeVal& newDuration) noexcept { }

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
std::vector<Process::ProcessDropHandler::ProcessDrop> DropHandler::drop(
    const QMimeData& data,
    const score::DocumentContext& ctx) const noexcept
{
  std::vector<Process::ProcessDropHandler::ProcessDrop> vec;

  if(!data.hasUrls())
      return vec;

  static const constexpr auto isSupportedImage = [] (const QFileInfo& filepath)
  {
      static const auto set = DropHandler{}.fileExtensions();
      return set.contains(filepath.completeSuffix().toLower());
  };
  Process::ProcessDropHandler::ProcessDrop p;
  p.creation.key = Metadata<ConcreteKey_k, Gfx::Images::Model>::get();
  p.setup = [files=data.urls()](Process::ProcessModel& m, score::Dispatcher& disp) {
      auto& proc = static_cast<Gfx::Images::Model&>(m);
      std::vector<Image> images;

      for (const auto& url : files)
      {
          QString filename = url.toLocalFile();
          if (!isSupportedImage(QFileInfo{filename}))
              continue;

          QImage img{filename};
          if(img.isNull() || img.size() == QSize{})
              continue;

          images.push_back({std::move(filename), std::move(img)});
      }
      disp.submit(new ChangeImages{proc, std::move(images)});
  };
  vec.push_back(std::move(p));
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
