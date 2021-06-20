#include "Process.hpp"

#include <Gfx/Images/ImageListChooser.hpp>
#include <Gfx/Graph/Node.hpp>
#include <Gfx/TexturePort.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>

#include <QFileInfo>
#include <QImageReader>
#include <gsl/span>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Gfx::Images::Model)
namespace Gfx
{
ossia::value fromImageSet(const gsl::span<score::gfx::Image>& images)
{
  std::vector<ossia::value> v;
  for(auto& img : images)
  {
    v.push_back(img.path.toStdString());
  }
  return v;
}

void releaseImages(std::vector<score::gfx::Image>& imgs)
{
  auto& cache = ImageCache::instance();
  for(auto& img : imgs)
    cache.release(std::move(img));
  imgs.clear();
}

std::vector<score::gfx::Image> getImages(const ossia::value& val)
{
  auto& cache = ImageCache::instance();
  std::vector<score::gfx::Image> imgs;
  for(auto& img : ossia::convert<std::vector<ossia::value>>(val))
  {
    if(auto image = cache.acquire(ossia::convert<std::string>(img)))
    {
      imgs.push_back(std::move(*image));
    }
  }
  return imgs;
}
}

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
  m_inlets.back()->setName(tr("Index"));
  {
    auto opacity = new Process::FloatSlider{Id<Process::Port>(1), this};
    opacity->setName(tr("Opacity"));
    opacity->setValue(1.);
    m_inlets.push_back(opacity);
  }
  {
    auto pos = new Process::XYSlider{Id<Process::Port>(2), this};
    pos->setName(tr("Position"));
    pos->setDomain(
      ossia::make_domain(ossia::vec2f{-5.0, 5.0}, ossia::vec2f{5.0, -5.0}));

    m_inlets.push_back(pos);
  }

  {
    auto scaleX = new Process::FloatSlider{Id<Process::Port>(3), this};
    scaleX->setName(tr("Scale X"));
    scaleX->setValue(1.);
    scaleX->setDomain(ossia::make_domain(-1., 10));
    m_inlets.push_back(scaleX);
  }
  {
    auto scaleY = new Process::FloatSlider{Id<Process::Port>(4), this};
    scaleY->setName(tr("Scale Y"));
    scaleY->setValue(1.);
    scaleY->setDomain(ossia::make_domain(-1., 10));
    m_inlets.push_back(scaleY);
  }


  {
    auto images = new ImageListChooser{{}, tr("Images"), Id<Process::Port>(5), this};
    m_inlets.push_back(images);
    connect(images, &ImageListChooser::valueChanged, this, &Model::on_imagesChanged);
  }

  m_outlets.push_back(new TextureOutlet{Id<Process::Port>(0), this});
}

Model::~Model() { }

void Model::on_imagesChanged(const ossia::value& v)
{
  auto imgs = getImages(safe_cast<ImageListChooser*>(inlets().back())->value());
  int count = 0;
  for (const auto& img : imgs)
    count += img.frames.size();

  auto spinbox = safe_cast<Process::IntSpinBox*>(m_inlets[0]);
  if (!imgs.empty())
    spinbox->setDomain(ossia::make_domain(int(0), int(count) - 1));
  else
    spinbox->setDomain(ossia::make_domain(int(0), int(0)));

  releaseImages(imgs);
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

static std::optional<score::gfx::Image> readImage(const QString& filename)
{
  QFileInfo info{filename};
  if (!isSupportedImage(info))
    return {};

  QImageReader reader{filename};
  std::vector<QImage> frames;
  while (reader.canRead())
  {
    QImage img = reader.read();

    if (img.isNull() || img.size() == QSize{})
      continue;

    frames.push_back(std::move(img));
  }

  if (frames.empty())
    return {};

  return score::gfx::Image{filename, std::move(frames)};
}

std::vector<Process::ProcessDropHandler::ProcessDrop> DropHandler::drop(
    const QMimeData& data,
    const score::DocumentContext& ctx) const noexcept
{
  std::vector<Process::ProcessDropHandler::ProcessDrop> vec;

  if (!data.hasUrls())
    return vec;

  Process::ProcessDropHandler::ProcessDrop p;
  p.creation.key = Metadata<ConcreteKey_k, Gfx::Images::Model>::get();
  p.setup = [files = data.urls()](
                Process::ProcessModel& m, score::Dispatcher& disp) {
    auto& proc = static_cast<Model&>(m);
    std::vector<score::gfx::Image> images;

    for (const auto& url : files)
    {
      if (auto img = readImage(url.toLocalFile()))
      {
        images.push_back(*std::move(img));
      }
    }

    if (!images.empty())
      disp.submit(new Process::SetControlValue{safe_cast<Process::ControlInlet&>(*proc.inlets().back()), fromImageSet(images)});
  };
  vec.push_back(std::move(p));
  return vec;
}
}
namespace Gfx
{

std::optional<score::gfx::Image> ImageCache::acquire(const std::string& path)
{
  if(auto it = m_images.find(path); it != m_images.end())
  {
    it->second.first++;
    return it->second.second;
  }

  if(auto img = Images::readImage(QString::fromStdString(path)))
  {
    auto [it, ok] = m_images.insert({path, {0, *std::move(img)}});
    return it->second.second;
  }
  return {};
}

void ImageCache::release(score::gfx::Image&& img)
{
  if(auto it = m_images.find(img.path.toStdString()); it != m_images.end())
  {
    it->second.first--;
    if(it->second.first <= 0)
    {
      m_images.erase(it);
    }
  }
}

ImageCache& ImageCache::instance() noexcept
{
  static ImageCache img;
  return img;
}

}
template <>
void DataStreamReader::read(const score::gfx::Image& proc)
{
  m_stream << proc.path;
}

template <>
void DataStreamWriter::write(score::gfx::Image& proc)
{
  m_stream >> proc.path;
  if (auto img = Gfx::Images::readImage(proc.path))
    proc = *std::move(img);
}

template <>
void JSONReader::read(const score::gfx::Image& proc)
{
  stream.StartObject();
  stream.Key("Path");
  stream.String(proc.path.toStdString());
  stream.EndObject();
}

template <>
void JSONWriter::write(score::gfx::Image& proc)
{
  const auto& obj = base.GetObject();
  proc.path = obj["Path"].GetString();
  if (auto img = Gfx::Images::readImage(proc.path))
    proc = *std::move(img);
}

template <>
void DataStreamReader::read(const Gfx::Images::Model& proc)
{
  readPorts(*this, proc.m_inlets, proc.m_outlets);

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

  checkDelimiter();
}

template <>
void JSONReader::read(const Gfx::Images::Model& proc)
{
  readPorts(*this, proc.m_inlets, proc.m_outlets);
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
}

template <> void
DataStreamReader::read<Gfx::Images::ImageListChooser>(const Gfx::Images::ImageListChooser& p)
{
  read((const Process::ControlInlet&)p);
}
template <> void
DataStreamWriter::write<Gfx::Images::ImageListChooser>(Gfx::Images::ImageListChooser& p)
{
}
template <>  void
JSONReader::read<Gfx::Images::ImageListChooser>(const Gfx::Images::ImageListChooser& p)
{
  read((const Process::ControlInlet&)p);
}
template <> void
JSONWriter::write<Gfx::Images::ImageListChooser>(Gfx::Images::ImageListChooser& p)
{
}
