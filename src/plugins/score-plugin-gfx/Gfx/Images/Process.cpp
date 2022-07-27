#include "Process.hpp"

#include <Process/Dataflow/ControlWidgets.hpp>
#include <Gfx/Images/ImageListChooser.hpp>
#include <Gfx/Graph/Node.hpp>
#include <Gfx/TexturePort.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>

#include <ossia/detail/logger.hpp>
#include <ossia/detail/span.hpp>
#include <ossia/network/value/format_value.hpp>

#include <QFileInfo>
#include <QImageReader>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Gfx::Images::Model)
namespace Gfx
{
ossia::value fromImageSet(const tcb::span<score::gfx::Image>& images)
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
  m_inlets.push_back(
        new Process::IntSpinBox{0, 0, 0, tr("Index"), Id<Process::Port>(0), this});
  {
    auto opacity =
        new Process::FloatSlider{0., 1., 1., tr("Opacity"), Id<Process::Port>(1), this};
    m_inlets.push_back(opacity);
  }
  {
    auto pos = new Process::XYSlider{
        ossia::vec2f{-5.0, -5.0},
        ossia::vec2f{5.0, 5.0},
        ossia::vec2f{0.0, 0.0},
        tr("Position"),
        Id<Process::Port>(2), this};
    m_inlets.push_back(pos);
  }

  {
    auto scaleX = new Process::FloatSlider{-1.f, 10.f, 1.f, tr("Scale X"), Id<Process::Port>(3), this};
    m_inlets.push_back(scaleX);
  }
  {
    auto scaleY = new Process::FloatSlider{-1.f, 10.f, 1.f, tr("Scale Y"), Id<Process::Port>(4), this};
    m_inlets.push_back(scaleY);
  }

  {
    auto images = new ImageListChooser{{}, tr("Images"), Id<Process::Port>(5), this};
    m_inlets.push_back(images);
    connect(images, &ImageListChooser::valueChanged, this, &Model::on_imagesChanged);
  }

  {
    std::vector<std::pair<QString, ossia::value>> combo{
      {"Single", (int) score::gfx::ImageMode::Single},
      {"Clamp", (int) score::gfx::ImageMode::Clamped},
      {"Tile", (int) score::gfx::ImageMode::Tiled},
      {"Mirror", (int) score::gfx::ImageMode::Mirrored},
    };
    auto tile = new Process::ComboBox{combo, 0, tr("Tile"), Id<Process::Port>(6), this};
    m_inlets.push_back(tile);
  }
  m_outlets.push_back(new TextureOutlet{Id<Process::Port>(0), this});
}

Model::~Model()
{
  releaseImages(m_currentImages);
}

void Model::on_imagesChanged(const ossia::value& v)
{
  releaseImages(m_currentImages);
  m_currentImages = getImages(safe_cast<ImageListChooser*>(m_inlets[5])->value());

  int count = 0;
  for (const auto& img : m_currentImages)
    count += img.frames.size();

  auto spinbox = safe_cast<Process::IntSpinBox*>(m_inlets[0]);
  if (!m_currentImages.empty())
    spinbox->setDomain(ossia::make_domain(int(0), int(count) - 1));
  else
    spinbox->setDomain(ossia::make_domain(int(0), int(0)));

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
  return {"png", "jpg", "jpe", "jpeg", "gif", "bmp"};
}

QSet<QString> DropHandler::fileExtensions() const noexcept
{
  return {"png", "jpg", "jpe", "jpeg", "gif", "bmp"};
}

static bool isSupportedImage(const QFileInfo& filepath)
{
  static const auto set = DropHandler{}.fileExtensions();
  return set.contains(filepath.suffix().toLower());
}

static std::optional<score::gfx::Image> readImage(const QString& filename)
{
  QFileInfo info{filename};
  if (!isSupportedImage(info))
    return {};

  QImageReader reader{filename};
  reader.setBackgroundColor(Qt::transparent);
  std::vector<QImage> frames;
  while (reader.canRead())
  {
    QImage img = reader.read();

    if (img.isNull() || img.size() == QSize{})
      continue;

    if (img.format() != QImage::Format_ARGB32)
      img.convertTo(QImage::Format_ARGB32);

    frames.push_back(std::move(img));
  }

  if (frames.empty())
    return {};

  return score::gfx::Image{filename, std::move(frames)};
}

void DropHandler::dropCustom(
    std::vector<ProcessDrop>& vec,
    const QMimeData& data,
    const score::DocumentContext& ctx) const noexcept
{
  if (!data.hasUrls())
    return;

  Process::ProcessDropHandler::ProcessDrop p;
  p.creation.key = Metadata<ConcreteKey_k, Gfx::Images::Model>::get();
  p.setup = [files = data.urls()](
                Process::ProcessModel& m, score::Dispatcher& disp) {
    auto& proc = static_cast<Model&>(m);
    std::vector<score::gfx::Image> images;

    for (const auto& url : files)
    {
      if (auto img = Gfx::ImageCache::instance().acquire(url.toLocalFile().toStdString()))
      {
        images.push_back(*std::move(img));
      }
    }

    if (!images.empty())
      disp.submit(new Process::SetControlValue{safe_cast<Process::ControlInlet&>(*proc.inlets()[5]), fromImageSet(images)});
  };
  vec.push_back(std::move(p));
  return;
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

  proc.on_imagesChanged(((Process::ControlInlet*)(proc.m_inlets[5]))->value());

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

  // Update to newer versions
  if(proc.m_inlets.size() < 7)
  {
      std::vector<std::pair<QString, ossia::value>> combo{
        {"Single", (int) score::gfx::ImageMode::Single},
        {"Clamp", (int) score::gfx::ImageMode::Clamped},
        {"Tile", (int) score::gfx::ImageMode::Tiled},
        {"Mirror", (int) score::gfx::ImageMode::Mirrored},
      };
      auto tile = new Process::ComboBox{combo, 0, QObject::tr("Tile"), Id<Process::Port>(6), &proc};
      proc.m_inlets.push_back(tile);
  }

  proc.on_imagesChanged(((Process::ControlInlet*)(proc.m_inlets[5]))->value());
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
