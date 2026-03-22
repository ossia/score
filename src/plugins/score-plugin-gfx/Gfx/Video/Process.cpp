#include "Process.hpp"

#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>

#include <Gfx/Graph/Node.hpp>
#include <Gfx/Settings/Model.hpp>
#include <Video/GpuFormats.hpp>
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
  conf.graphicsApi = static_cast<int>(set.graphicsApiEnum());
  if(auto hw = set.getHardwareDecode(); !hw.isEmpty() && hw != decoders.None)
  {
    if(hw == decoders.Auto)
    {
      // "Auto" mode: let DirectVideoNodeRenderer::selectHardwareAcceleration()
      // pick the best backend per graphics API and codec at runtime.
      // For the background-thread VideoDecoder path, we signal "auto" via
      // a special sentinel value; the decoder will use the platform-preferred
      // acceleration. For now, pick a reasonable default per platform.
#if defined(_WIN32)
      // Match the RHI backend when possible
      if(conf.graphicsApi == static_cast<int>(score::gfx::D3D12))
      {
#if defined(_WIN32) && LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(58, 29, 100)
        conf.hardwareAcceleration = AV_PIX_FMT_D3D12;
#else
        conf.hardwareAcceleration = AV_PIX_FMT_D3D11;
#endif
      }
      else
        conf.hardwareAcceleration = AV_PIX_FMT_D3D11;
#elif defined(__APPLE__)
      conf.hardwareAcceleration = AV_PIX_FMT_VIDEOTOOLBOX;
#elif defined(__linux__)
      if(::Video::hardwareDecoderIsAvailable(AV_PIX_FMT_VAAPI))
        conf.hardwareAcceleration = AV_PIX_FMT_VAAPI;
      else if(::Video::hardwareDecoderIsAvailable(AV_PIX_FMT_CUDA))
        conf.hardwareAcceleration = AV_PIX_FMT_CUDA;
      else if(::Video::hardwareDecoderIsAvailable(AV_PIX_FMT_VULKAN))
        conf.hardwareAcceleration = AV_PIX_FMT_VULKAN;
#endif
    }
    else if(hw == decoders.CUDA)
      conf.hardwareAcceleration = AV_PIX_FMT_CUDA;
    else if(hw == decoders.QSV)
      conf.hardwareAcceleration = AV_PIX_FMT_QSV;
    else if(hw == decoders.VDPAU)
      conf.hardwareAcceleration = AV_PIX_FMT_VDPAU;
    else if(hw == decoders.VAAPI)
      conf.hardwareAcceleration = AV_PIX_FMT_VAAPI;
    else if(hw == decoders.D3D)
      conf.hardwareAcceleration = AV_PIX_FMT_D3D11;
#if defined(_WIN32) && LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(58, 29, 100)
    else if(hw == decoders.D3D12)
      conf.hardwareAcceleration = AV_PIX_FMT_D3D12;
#endif
    else if(hw == decoders.DXVA)
      conf.hardwareAcceleration = AV_PIX_FMT_DXVA2_VLD;
    else if(hw == decoders.VideoToolbox)
      conf.hardwareAcceleration = AV_PIX_FMT_VIDEOTOOLBOX;
    else if(hw == decoders.V4L2)
      conf.hardwareAcceleration = AV_PIX_FMT_DRM_PRIME;
    else if(hw == decoders.VulkanVideo)
      conf.hardwareAcceleration = AV_PIX_FMT_VULKAN;
  }
  return conf;
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
  setSize(QSizeF{size().width(), 54});

  m_outlets.push_back(new TextureOutlet{"Video Out", Id<Process::Port>(0), this});
}

Model::~Model() { }

std::shared_ptr<video_decoder> Model::makeDecoder() const noexcept
try
{
  auto dec = std::make_shared<video_decoder>(videoDecoderConfiguration());
  if(!dec->load(absolutePath().toStdString()))
    return {};
  return dec;
}
catch(...)
{
  return {};
}

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

score::gfx::PlaybackMode Model::playbackMode() const noexcept
{
  return m_playbackMode;
}

void Model::setPlaybackMode(score::gfx::PlaybackMode t)
{
  if(t != m_playbackMode)
  {
    m_playbackMode = t;
    playbackModeChanged(t);
  }
}

::Video::OutputFormat Model::outputFormat() const noexcept
{
  return m_outputFormat;
}

void Model::setOutputFormat(::Video::OutputFormat t)
{
  if(t != m_outputFormat)
  {
    m_outputFormat = t;
    outputFormatChanged(t);
  }
}

::Video::Tonemap Model::tonemap() const noexcept
{
  return m_tonemap;
}

void Model::setTonemap(::Video::Tonemap t)
{
  if(t != m_tonemap)
  {
    m_tonemap = t;
    tonemapChanged(t);
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
  return {"mkv", "mov", "mp4", "h264", "avi", "hap",  "mpg", "mpeg",
          "imf", "mxf", "mts", "m2ts", "mj2", "webm", "y4m", "nut", "ts"};
}

QSet<QString> DropHandler::fileExtensions() const noexcept
{
  return {"mkv", "mov", "mp4", "h264", "avi", "hap",  "mpg", "mpeg",
          "imf", "mxf", "mts", "m2ts", "mj2", "webm", "y4m", "nut", "ts"};
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
           << proc.m_ignoreTempo << proc.m_playbackMode
           << proc.m_outputFormat << proc.m_tonemap;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Gfx::Video::Model& proc)
{
  writePorts(
      *this, components.interfaces<Process::PortFactoryList>(), proc.m_inlets,
      proc.m_outlets, &proc);

  QString path;
  m_stream >> path >> proc.m_scaleMode >> proc.m_nativeTempo >> proc.m_ignoreTempo
      >> proc.m_playbackMode >> proc.m_outputFormat >> proc.m_tonemap;
  proc.setPath(path);
  checkDelimiter();
}

template <>
void JSONReader::read(const Gfx::Video::Model& proc)
{
  readPorts(*this, proc.m_inlets, proc.m_outlets);
  obj["FilePath"] = proc.m_path;
  obj["Scale"] = (int)proc.m_scaleMode;
  obj["Playback"] = (int)proc.m_playbackMode;
  obj["Tempo"] = proc.m_nativeTempo;
  obj["IgnoreTempo"] = proc.m_ignoreTempo;
  obj["OutputFormat"] = (int)proc.m_outputFormat;
  obj["Tonemap"] = (int)proc.m_tonemap;
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

  if(auto pb = obj.tryGet("Playback"))
    proc.m_playbackMode = static_cast<score::gfx::PlaybackMode>(pb->toInt());

  if(auto pb = obj.tryGet("OutputFormat"))
    proc.m_outputFormat = static_cast<::Video::OutputFormat>(pb->toInt());
  else
    proc.m_outputFormat = ::Video::OutputFormat::SDR;

  if(auto pb = obj.tryGet("Tonemap"))
    proc.m_tonemap = static_cast<::Video::Tonemap>(pb->toInt());
  else
    proc.m_tonemap = ::Video::Tonemap::Clamp;

  proc.m_nativeTempo = obj["Tempo"].toDouble();
  proc.m_ignoreTempo = obj["IgnoreTempo"].toBool();
}
