#include "Process.hpp"

#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>

#include <Gfx/Graph/Node.hpp>
#include <Gfx/TexturePort.hpp>
#include <Media/Tempo.hpp>

#include <QFileInfo>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Gfx::Video::Model)
namespace Gfx::Video
{

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

  m_outlets.push_back(new TextureOutlet{Id<Process::Port>(0), this});
}

Model::~Model() { }

void Model::setPath(const QString& f)
{
  if(f == m_path)
    return;

  m_path = f;
  m_decoder = std::make_shared<video_decoder>();
  m_decoder->load(m_path.toStdString(), 60.);
  setLoopDuration(TimeVal{m_decoder->duration()});
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
          "mpeg", "imf", "mxf", "mts",  "m2ts", "mj2"};
}

QSet<QString> DropHandler::fileExtensions() const noexcept
{
  return {"mkv",  "mov", "mp4", "h264", "avi",  "hap", "mpg",
          "mpeg", "imf", "mxf", "mts",  "m2ts", "mj2"};
}

std::optional<TimeVal> guessVideoDuration(const QString& path)
{
  AVFormatContext* ctx = avformat_alloc_context();
  int ret = avformat_open_input(&ctx, path.toStdString().c_str(), nullptr, nullptr);
  if(ret == 0)
  {
    avformat_find_stream_info(ctx, nullptr);
    int64_t duration = ctx->duration;
    avformat_close_input(&ctx);
    avformat_free_context(ctx);
    auto flicks_per_av_time_base = ossia::flicks_per_second<double> / AV_TIME_BASE;
    return TimeVal{(int64_t)(duration * flicks_per_av_time_base)};
  }
  else
  {
    avformat_close_input(&ctx);
    avformat_free_context(ctx);
    return std::nullopt;
  }
}

void DropHandler::dropPath(
    std::vector<ProcessDrop>& vec, const QString& filename,
    const score::DocumentContext& ctx) const noexcept
{
  Process::ProcessDropHandler::ProcessDrop p;
  p.creation.key = Metadata<ConcreteKey_k, Gfx::Video::Model>::get();
  p.creation.prettyName = QFileInfo{filename}.baseName();
  p.creation.customData = filename;

  // Invalid duration -> means that we could not open the file or do anything useful from it
  if((p.duration = guessVideoDuration(filename)))
    vec.push_back(std::move(p));
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
