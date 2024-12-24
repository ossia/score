#include "Process.hpp"

#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>

#include <Gfx/Graph/Node.hpp>
#include <Gfx/Settings/Model.hpp>
#include <Gfx/TexturePort.hpp>
#include <Media/Commands/ChangeAudioFile.hpp>
#include <Media/Sound/SoundModel.hpp>
#include <Media/Tempo.hpp>

#include <score/tools/File.hpp>

#include <QFileInfo>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Gfx::Video::Model)
namespace Gfx::Video
{
struct VideoProps
{
  TimeVal duration;

  struct Stream
  {
    AVMediaType type;
    std::string name;
  };
  std::vector<AVMediaType> streams;
};

static std::optional<VideoProps> guessVideoProps(const QString& path)
{
  AVFormatContext* ctx = avformat_alloc_context();
  int ret = avformat_open_input(&ctx, path.toStdString().c_str(), nullptr, nullptr);
  if(ret == 0)
  {
    avformat_find_stream_info(ctx, nullptr);
    if(ctx->nb_streams == 0)
      return std::nullopt;

    VideoProps ret;

    // Parse streams
    for(std::size_t i = 0; i < ctx->nb_streams; i++)
    {
      ret.streams.push_back(ctx->streams[i]->codecpar->codec_type);
    }

    // Parse duration
    int64_t duration = ctx->duration;
    auto flicks_per_av_time_base = ossia::flicks_per_second<double> / AV_TIME_BASE;
    ret.duration = TimeVal{(int64_t)(duration * flicks_per_av_time_base)};

    avformat_close_input(&ctx);
    avformat_free_context(ctx);

    return ret;
  }
  else
  {
    avformat_close_input(&ctx);
    avformat_free_context(ctx);
    return std::nullopt;
  }
}

static ::Video::DecoderConfiguration videoDecoderConfiguration() noexcept
{
  static const Gfx::Settings::HardwareVideoDecoder decoders;
  ::Video::DecoderConfiguration conf;
  auto& set = score::AppContext().settings<Gfx::Settings::Model>();
  conf.decoder = "";
  if(auto hw = set.getHardwareDecode(); !hw.isEmpty() && hw != decoders.None)
  {
    if(hw == decoders.CUDA)
      conf.hardwareAcceleration = AV_PIX_FMT_CUDA;
    else if(hw == decoders.QSV)
      conf.hardwareAcceleration = AV_PIX_FMT_QSV;
    else if(hw == decoders.VDPAU)
      conf.hardwareAcceleration = AV_PIX_FMT_VDPAU;
    else if(hw == decoders.VAAPI)
      conf.hardwareAcceleration = AV_PIX_FMT_VAAPI;
    else if(hw == decoders.D3D)
      conf.hardwareAcceleration = AV_PIX_FMT_D3D11;
    else if(hw == decoders.DXVA)
      conf.hardwareAcceleration = AV_PIX_FMT_DXVA2_VLD;
    else if(hw == decoders.VideoToolbox)
      conf.hardwareAcceleration = AV_PIX_FMT_VIDEOTOOLBOX;
    else if(hw == decoders.V4L2)
      conf.hardwareAcceleration = AV_PIX_FMT_DRM_PRIME;
  }
  return conf;
}

std::shared_ptr<video_decoder> makeDecoder(const std::string& absolutePath) noexcept
try
{
  auto dec = std::make_shared<video_decoder>(videoDecoderConfiguration());
  if(!dec->load(absolutePath))
    return {};
  return dec;
}
catch(...)
{
  return {};
}

Model::Model(
    const TimeVal& duration, const QString& path, const Id<Process::ProcessModel>& id,
    QObject* parent)
    : Process::ProcessModel{duration, id, "VideoProcess", parent}
    , m_nativeTempo{120}
{
  metadata().setInstanceName(*this);
  setLoops(true);
  setNativeTempo(Media::tempoAtStartDate(*this));
  setPath(path);

  m_inlets.push_back(new Process::VideoFileChooser{Id<Process::Port>(0), this});
  m_outlets.push_back(new TextureOutlet{Id<Process::Port>(0), this});
}

Model::~Model() { }

QString Model::absolutePath() const noexcept
{
  return score::locateFilePath(m_path, score::IDocument::documentContext(*this));
}

void Model::setPath(const QString& f)
{
  if(f == m_path)
    return;

  m_path = f;

  {
    // FIXME store the metadatas in cache instead of reopening the video every time
    video_decoder decoder(videoDecoderConfiguration());
    decoder.open(absolutePath().toStdString());

    setLoopDuration(TimeVal{decoder.duration()});
  }
  pathChanged(f);
}

score::gfx::ScaleMode Model::scaleMode() const noexcept
{
  return m_scaleMode;
}

void Model::setScaleMode(score::gfx::ScaleMode t)
{
  if(t != m_scaleMode)
  {
    m_scaleMode = t;
    scaleModeChanged(t);
  }
}

double Model::nativeTempo() const noexcept
{
  return m_nativeTempo;
}

void Model::setNativeTempo(double t)
{
  if(t != m_nativeTempo)
  {
    m_nativeTempo = t;
    nativeTempoChanged(t);
  }
}

bool Model::ignoreTempo() const noexcept
{
  return m_ignoreTempo;
}

void Model::setIgnoreTempo(bool t)
{
  if(t != m_ignoreTempo)
  {
    m_ignoreTempo = t;
    ignoreTempoChanged(t);
  }
}

QSet<QString> DropHandler::mimeTypes() const noexcept
{
  return {}; // TODO
}

QSet<QString> LibraryHandler::acceptedFiles() const noexcept
{
  return {"mkv",  "mov", "mp4", "h264", "avi",  "hap", "mpg",
          "mpeg", "imf", "mxf", "mts",  "m2ts", "mj2", "webm"};
}

QSet<QString> DropHandler::fileExtensions() const noexcept
{
  return {"mkv",  "mov", "mp4", "h264", "avi",  "hap", "mpg",
          "mpeg", "imf", "mxf", "mts",  "m2ts", "mj2", "webm"};
}

void DropHandler::dropPath(
    std::vector<ProcessDrop>& vec, const score::FilePath& filename,
    const score::DocumentContext& ctx) const noexcept
{
  Process::ProcessDropHandler::ProcessDrop p;
  p.creation.prettyName = filename.basename;
  p.creation.customData = filename.relative;

  if(auto props = guessVideoProps(filename.absolute))
  {
    p.duration = props->duration;

    // First all the video streams
    for(std::size_t i = 0; i < props->streams.size(); i++)
    {
      if(props->streams[i] == AVMEDIA_TYPE_VIDEO)
      {
        p.creation.key = Metadata<ConcreteKey_k, Gfx::Video::Model>::get();
        p.setup = {};
        vec.push_back(p);
      }
    }

    // Then all the audio streams
    for(std::size_t i = 0; i < props->streams.size(); i++)
    {
      if(props->streams[i] == AVMEDIA_TYPE_AUDIO)
      {
        p.creation.key = Metadata<ConcreteKey_k, Media::Sound::ProcessModel>::get();
        p.setup = [i](Process::ProcessModel& proc, score::Dispatcher& disp) {
          auto& p = safe_cast<Media::Sound::ProcessModel&>(proc);
          disp.submit(new Media::ChangeStream(p, (int)i));
        };
        vec.push_back(p);
      }
    }
  }
}

}
template <>
void DataStreamReader::read(const Gfx::Video::Model& proc)
{
  readPorts(*this, proc.m_inlets, proc.m_outlets);

  m_stream << proc.m_path << proc.m_scaleMode << proc.m_nativeTempo
           << proc.m_ignoreTempo;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Gfx::Video::Model& proc)
{
  writePorts(
      *this, components.interfaces<Process::PortFactoryList>(), proc.m_inlets,
      proc.m_outlets, &proc);

  QString path;
  m_stream >> path >> proc.m_scaleMode >> proc.m_nativeTempo >> proc.m_ignoreTempo;
  proc.setPath(path);
  checkDelimiter();
}

template <>
void JSONReader::read(const Gfx::Video::Model& proc)
{
  readPorts(*this, proc.m_inlets, proc.m_outlets);
  obj["FilePath"] = proc.m_path;
  obj["Scale"] = (int)proc.m_scaleMode;
  obj["Tempo"] = proc.m_nativeTempo;
  obj["IgnoreTempo"] = proc.m_ignoreTempo;
}

template <>
void JSONWriter::write(Gfx::Video::Model& proc)
{
  writePorts(
      *this, components.interfaces<Process::PortFactoryList>(), proc.m_inlets,
      proc.m_outlets, &proc);
  proc.setPath(obj["FilePath"].toString());

  if(auto sc = obj.tryGet("Scale"))
    proc.m_scaleMode = static_cast<score::gfx::ScaleMode>(sc->toInt());

  proc.m_nativeTempo = obj["Tempo"].toDouble();
  proc.m_ignoreTempo = obj["IgnoreTempo"].toBool();
}
