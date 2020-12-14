#include "Process.hpp"

#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>

#include <QFileInfo>
#include <QImageReader>
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
  {
    auto opacity = new Process::FloatSlider{Id<Process::Port>(1), this};
    opacity->setCustomData(tr("Opacity"));
    opacity->setValue(1.);
    m_inlets.push_back(opacity);
  }
  m_inlets.push_back(new Process::XYSlider{Id<Process::Port>(2), this});
  m_inlets.back()->setCustomData(tr("Position"));
  {
    auto scale = new Process::XYSlider{Id<Process::Port>(3), this};
    scale->setCustomData(tr("Scale"));
    scale->setValue(ossia::vec2f{1., 1.});
    scale->setDomain(ossia::make_domain(ossia::vec2f{0.01, 0.01}, ossia::vec2f{10., 10.}));
    m_inlets.push_back(scale);
  }

  m_outlets.push_back(new TextureOutlet{Id<Process::Port>(0), this});
}

Model::~Model() { }

void Model::setImages(const std::vector<Image>& f)
{
  m_images = f;

  int count = 0;
  for(const auto& img : m_images)
    count += img.frames.size();

  auto spinbox = safe_cast<Process::IntSpinBox*>(m_inlets[0]);
  if(!f.empty())
    spinbox->setDomain(ossia::make_domain(int(0), int(count) - 1));
  else
    spinbox->setDomain(ossia::make_domain(int(0), int(0)));

  imagesChanged();
}

QString Model::prettyName() const noexcept
{
  return tr("Images");
}

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

static bool isSupportedImage(const QFileInfo& filepath)
{
    static const auto set = DropHandler{}.fileExtensions();
    return set.contains(filepath.completeSuffix().toLower());
}

static std::optional<Image> readImage(const QString& filename)
{
  QFileInfo info{filename};
  if (!isSupportedImage(info))
    return {};

  QImageReader reader{filename};
  std::vector<QImage> frames;
  while(reader.canRead())
  {
    QImage img = reader.read();

    if(img.isNull() || img.size() == QSize{})
      continue;

    frames.push_back(std::move(img));
  }

  if(frames.empty())
    return {};

  return Image{filename, std::move(frames)};
}

std::vector<Process::ProcessDropHandler::ProcessDrop> DropHandler::drop(
    const QMimeData& data,
    const score::DocumentContext& ctx) const noexcept
{
  std::vector<Process::ProcessDropHandler::ProcessDrop> vec;

  if(!data.hasUrls())
      return vec;

  Process::ProcessDropHandler::ProcessDrop p;
  p.creation.key = Metadata<ConcreteKey_k, Gfx::Images::Model>::get();
  p.setup = [files=data.urls()](Process::ProcessModel& m, score::Dispatcher& disp) {
      auto& proc = static_cast<Gfx::Images::Model&>(m);
      std::vector<Image> images;

      for (const auto& url : files)
      {
        if(auto img = readImage(url.toLocalFile()))
        {
          images.push_back(*std::move(img));
        }
      }

      if(!images.empty())
        disp.submit(new ChangeImages{proc, std::move(images)});
  };
  vec.push_back(std::move(p));
  return vec;
}

}
template <>
void DataStreamReader::read(const Gfx::Image& proc)
{
  m_stream << proc.path;
}

template <>
void DataStreamWriter::write(Gfx::Image& proc)
{
  m_stream >> proc.path;
  if(auto img = Gfx::Images::readImage(proc.path))
    proc = *std::move(img);
}

template <>
void JSONReader::read(const Gfx::Image& proc)
{
  stream.StartObject();
  stream.Key("Path");
  stream.String(proc.path.toStdString());
  stream.EndObject();
}

template <>
void JSONWriter::write(Gfx::Image& proc)
{
  const auto& obj = base.GetObject();
  proc.path = obj["Path"].GetString();
  if(auto img = Gfx::Images::readImage(proc.path))
    proc = *std::move(img);
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
  obj["Images"] = proc.m_images;
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
  proc.m_images <<= obj["Images"];
}
