#include "Process.hpp"

#include <Process/Dataflow/ControlWidgets.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>
#include <Process/ExternalFiles.hpp>

#include <Gfx/Graph/Node.hpp>
#include <Gfx/Images/ImageListChooser.hpp>
#include <Gfx/TexturePort.hpp>

#include <ossia/detail/logger.hpp>
#include <span>
#include <ossia/network/value/format_value.hpp>

#include <QDebug>
#include <QFileInfo>
#include <QImageReader>
#include <QMimeDatabase>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Gfx::Images::Model)
namespace Gfx
{
ossia::value fromImageSet(const std::span<score::gfx::Image>& images)
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

std::vector<score::gfx::Image>
getImages(const ossia::value& val, const score::DocumentContext& ctx)
{
  auto& cache = ImageCache::instance();
  std::vector<score::gfx::Image> imgs;
  for(auto& img : ossia::convert<std::vector<ossia::value>>(val))
  {
    auto image_path = QString::fromStdString(ossia::convert<std::string>(img));
    image_path = score::locateFilePath(image_path, ctx);
    if(auto image = cache.acquire(image_path))
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
    const TimeVal& duration, const Id<Process::ProcessModel>& id, QObject* parent)
    : Process::ProcessModel{duration, id, "gfxProcess", parent}
{
  metadata().setInstanceName(*this);
  m_inlets.push_back(
      new Process::IntSpinBox{0, 0, 0, tr("Index"), Id<Process::Port>(0), this});
  {
    auto opacity = new Process::FloatSlider{
        0., 1., 1., tr("Opacity"), Id<Process::Port>(1), this};
    m_inlets.push_back(opacity);
  }
  {
    auto pos = new Process::XYSlider{ossia::vec2f{-5.0, -5.0}, ossia::vec2f{5.0, 5.0},
                                     ossia::vec2f{0.0, 0.0},   tr("Position"),
                                     Id<Process::Port>(2),     this};
    m_inlets.push_back(pos);
  }

  {
    auto scaleX = new Process::FloatSlider{
        -1.f, 10.f, 1.f, tr("Scale X"), Id<Process::Port>(3), this};
    m_inlets.push_back(scaleX);
  }
  {
    auto scaleY = new Process::FloatSlider{
        -1.f, 10.f, 1.f, tr("Scale Y"), Id<Process::Port>(4), this};
    m_inlets.push_back(scaleY);
  }

  {
    auto images = new ImageListChooser{{}, tr("Images"), Id<Process::Port>(5), this};
    m_inlets.push_back(images);
    connect(images, &ImageListChooser::valueChanged, this, &Model::on_imagesChanged);
  }

  {
    std::vector<std::pair<QString, ossia::value>> combo{
        {"Single", (int)score::gfx::ImageMode::Single},
        {"Clamp", (int)score::gfx::ImageMode::Clamped},
        {"Tile", (int)score::gfx::ImageMode::Tiled},
        {"Mirror", (int)score::gfx::ImageMode::Mirrored},
    };
    auto tile = new Process::ComboBox{combo, 0, tr("Tile"), Id<Process::Port>(6), this};
    m_inlets.push_back(tile);
  }

  {
    std::vector<std::pair<QString, ossia::value>> combo{
        {"Original", (int)score::gfx::ScaleMode::Original},
        {"Black bars", (int)score::gfx::ScaleMode::BlackBars},
        {"Fill", (int)score::gfx::ScaleMode::Fill},
        {"Stretch", (int)score::gfx::ScaleMode::Stretch},
    };
    auto tile = new Process::ComboBox{combo, 0, tr("Scale"), Id<Process::Port>(7), this};
    m_inlets.push_back(tile);
  }
  m_outlets.push_back(new TextureOutlet{"Texture Out", Id<Process::Port>(0), this});
}

Model::~Model()
{
  releaseImages(m_currentImages);
}

void Model::mapExternalFiles(Process::ExternalFileMap& map)
{
  Process::ProcessModel::mapExternalFiles(map);

  // The image list is a single control holding N paths; ExternalFileMap
  // rewrites them element per element and emits one command for the list.
  for(auto* inlet : m_inlets)
    if(auto* images = dynamic_cast<ImageListChooser*>(inlet))
      map.control(*images, score::FileKind::Image);
}

void Model::on_imagesChanged(const ossia::value& v)
{
  releaseImages(m_currentImages);
  m_currentImages = getImages(
      safe_cast<ImageListChooser*>(m_inlets[5])->value(),
      score::IDocument::documentContext(*this));

  int count = 0;
  for(const auto& img : m_currentImages)
    count += img.frames.size();

  auto spinbox = safe_cast<Process::IntSpinBox*>(m_inlets[0]);
  if(!m_currentImages.empty())
    spinbox->setDomain(ossia::make_domain(int(0), int(count) - 1));
  else
    spinbox->setDomain(ossia::make_domain(int(0), int(0)));
}

void Model::finishLoad()
{
  // Old documents predate the Tile / Scale ports; the executor and the
  // render node index inlets positionally, so bring the list up to date
  // (the JSON path used to do this inline; the DataStream path never did,
  // which made the executor index out of bounds on old .scorebin files).
  if(m_inlets.size() < 7)
  {
    std::vector<std::pair<QString, ossia::value>> combo{
        {"Single", (int)score::gfx::ImageMode::Single},
        {"Clamp", (int)score::gfx::ImageMode::Clamped},
        {"Tile", (int)score::gfx::ImageMode::Tiled},
        {"Mirror", (int)score::gfx::ImageMode::Mirrored},
    };
    m_inlets.push_back(
        new Process::ComboBox{combo, 0, tr("Tile"), Id<Process::Port>(6), this});
  }

  if(m_inlets.size() < 8)
  {
    std::vector<std::pair<QString, ossia::value>> combo{
        {"Original", (int)score::gfx::ScaleMode::Original},
        {"Black bars", (int)score::gfx::ScaleMode::BlackBars},
        {"Fill", (int)score::gfx::ScaleMode::Fill},
        {"Stretch", (int)score::gfx::ScaleMode::Stretch},
    };
    m_inlets.push_back(
        new Process::ComboBox{combo, 0, tr("Scale"), Id<Process::Port>(7), this});
  }

  if(auto* images = dynamic_cast<ImageListChooser*>(m_inlets[5]))
  {
    connect(images, &ImageListChooser::valueChanged, this, &Model::on_imagesChanged);
    on_imagesChanged(images->value());
  }
}

QString Model::prettyName() const noexcept
{
  return tr("Images");
}

QSet<QString> DropHandler::mimeTypes() const noexcept
{
  return {}; // TODO
}

const QSet<QString>& supportedImageExtensions()
{
  // What this exact Qt build decodes — QtGui's built-in formats plus the
  // linked imageformat plugins — instead of a hardcoded list that both
  // missed linked plugins (webp, ico, tif...) and promised formats no
  // plugin provides (heic, jp2 in the static SDK build).
  static const QSet<QString> set = [] {
    QSet<QString> s;
    QMimeDatabase db;
    for(const auto& mime : QImageReader::supportedMimeTypes())
      for(const auto& suffix : db.mimeTypeForName(QString::fromUtf8(mime)).suffixes())
        s.insert(suffix.toLower());
    for(const auto& fmt : QImageReader::supportedImageFormats())
      s.insert(QString::fromUtf8(fmt).toLower());
    return s;
  }();
  return set;
}

QSet<QString> LibraryHandler::acceptedFiles() const noexcept
{
  return supportedImageExtensions();
}

QSet<QString> DropHandler::fileExtensions() const noexcept
{
  return supportedImageExtensions();
}

static bool isSupportedImage(const QFileInfo& filepath)
{
  if(supportedImageExtensions().contains(filepath.suffix().toLower()))
    return true;
  // Content sniff for misnamed files; also the diagnostic gate for the
  // qWarning below.
  return !QImageReader::imageFormat(filepath.filePath()).isEmpty();
}

static std::optional<score::gfx::Image> readImage(const QString& filename)
{
  QFileInfo info{filename};
  if(!isSupportedImage(info))
  {
    qWarning() << "Images: unsupported or unreadable file:" << filename;
    return {};
  }

  QImageReader reader{filename};
  reader.setBackgroundColor(Qt::transparent);
  std::vector<QImage> frames;
  // Animations (gif) auto-advance on read(); multi-image formats (ico,
  // multi-page tiff) do NOT — canRead() stays true and read() returns the
  // same frame forever unless we jump explicitly. Break on failed reads too
  // (a truncated animation keeps canRead() true).
  while(reader.canRead() && frames.size() < 4096)
  {
    const int cur = reader.currentImageNumber();
    QImage img = reader.read();

    if(img.isNull() || img.size() == QSize{})
      break;

    if(img.format() != QImage::Format_ARGB32)
      img.convertTo(QImage::Format_ARGB32);

    frames.push_back(std::move(img));

    if(reader.currentImageNumber() == cur && !reader.jumpToNextImage())
      break;
  }

  if(frames.empty())
  {
    qWarning() << "Images: could not decode:" << filename << reader.errorString();
    return {};
  }

  return score::gfx::Image{filename, std::move(frames)};
}

void DropHandler::dropCustom(
    std::vector<ProcessDrop>& vec, const QMimeData& data,
    const score::DocumentContext& ctx) const noexcept
{
  if(!data.hasUrls())
    return;

  Process::ProcessDropHandler::ProcessDrop p;
  p.creation.key = Metadata<ConcreteKey_k, Gfx::Images::Model>::get();
  p.setup = [files = data.urls()](Process::ProcessModel& m, score::Dispatcher& disp) {
    auto& proc = static_cast<Model&>(m);
    std::vector<score::gfx::Image> images;

    for(const auto& url : files)
    {
      if(auto img = Gfx::ImageCache::instance().acquire(url.toLocalFile()))
      {
        images.push_back(*std::move(img));
      }
    }

    if(!images.empty())
      disp.submit(new Process::SetControlValue{
          safe_cast<Process::ControlInlet&>(*proc.inlets()[5]), fromImageSet(images)});
  };
  vec.push_back(std::move(p));
  return;
}
}
namespace Gfx
{

std::optional<score::gfx::Image> ImageCache::acquire(const QString& path)
{
  if(auto it = m_images.find(path); it != m_images.end())
  {
    it->second.first++;
    return it->second.second;
  }

  if(auto img = Images::readImage(path))
  {
    auto [it, ok] = m_images.insert({path, {1, *std::move(img)}});
    return it->second.second;
  }
  return {};
}

void ImageCache::release(score::gfx::Image&& img)
{
  if(auto it = m_images.find(img.path); it != m_images.end())
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
  if(auto img = Gfx::Images::readImage(proc.path))
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
  if(auto img = Gfx::Images::readImage(proc.path))
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
      *this, components.interfaces<Process::PortFactoryList>(), proc.m_inlets,
      proc.m_outlets, &proc);

  proc.finishLoad();

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
      *this, components.interfaces<Process::PortFactoryList>(), proc.m_inlets,
      proc.m_outlets, &proc);

  proc.finishLoad();
}

static std::vector<ossia::value>
imagePathsToRelative(const Gfx::Images::ImageListChooser& p)
{
  // Hack: we temporarily change the value to relative paths
  auto& ctx = score::IDocument::documentContext(p);
  auto values = ossia::convert<std::vector<ossia::value>>(p.value());
  for(auto& v : values)
  {
    v = score::relativizeFilePath(
            QString::fromStdString(ossia::convert<std::string>(v)), ctx)
            .toStdString();
  }
  return values;
}
static std::vector<ossia::value> imagePathsToAbsolute(
    const Gfx::Images::ImageListChooser& p, std::vector<ossia::value> values)
{
  // Hack: we temporarily change the value to relative paths
  auto& ctx = score::IDocument::documentContext(p);
  for(auto& v : values)
  {
    v = score::locateFilePath(
            QString::fromStdString(ossia::convert<std::string>(v)), ctx)
            .toStdString();
  }
  return values;
}
template <>
void DataStreamReader::read(
    const Gfx::Images::ImageListChooser& p)
{
  read((const Process::ControlInlet&)p);
  readFrom(imagePathsToRelative(p));
}
template <>
void DataStreamWriter::write(
    Gfx::Images::ImageListChooser& p)
{
  std::vector<ossia::value> values;
  writeTo(values);
  p.m_value = imagePathsToAbsolute(p, values);
}

template <>
void JSONReader::read(
    const Gfx::Images::ImageListChooser& p)
{
  obj[strings.Value] = ossia::value(imagePathsToRelative(p));
  obj[strings.Domain] = p.m_domain;
}

template <>
void JSONWriter::write(Gfx::Images::ImageListChooser& p)
{
  p.m_value
      = imagePathsToAbsolute(p, ossia::convert<std::vector<ossia::value>>(p.m_value));
}
