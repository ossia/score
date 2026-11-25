#include "TextureRecorder.hpp"
#include "VideoEncoding.hpp"

#include <Media/Libav.hpp>

#include <Process/Dataflow/Cable.hpp>
#include <Process/Dataflow/CableData.hpp>
#include <Process/Dataflow/Port.hpp>

#include <Device/Protocol/DeviceInterface.hpp>

#include <Explorer/DeviceList.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/GfxContext.hpp>
#include <Gfx/GfxInputDevice.hpp>
#include <Gfx/Libav/LibavEncoder.hpp>
#include <Gfx/Libav/LibavEncoderNode.hpp>
#include <Gfx/Libav/LibavOutputStream.hpp>
#include <Gfx/TexturePort.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/model/path/PathSerialization.hpp>

#include <QTimer>

#if SCORE_HAS_LIBAV
extern "C" {
#include <libavcodec/avcodec.h>
}
#endif

namespace Recording
{
QString VideoSettings::extension() const
{
  if(auto e = videoEncodingFromJson(encoder))
    return extensionOfMuxer(e->muxer);
  switch(codec)
  {
    case Hap:
    case ProRes:
      return QStringLiteral("mov");
    default:
      return QStringLiteral("mkv");
  }
}

namespace
{
#if SCORE_HAS_LIBAV
bool hasEncoder(const char* name)
{
  return avcodec_find_encoder_by_name(name) != nullptr;
}

//! The encoder output's settings; a codec FFmpeg lacks falls back to FFV1,
//! which every FFmpeg has
Gfx::LibavOutputSettings outputSettings(const QString& file, const VideoSettings& v)
{
  // The FFmpeg output device's settings, as chosen
  if(auto e = videoEncodingFromJson(v.encoder))
  {
    auto s = e->toOutputSettings();
    s.path = file;
    s.audio_channels = 0;
    s.audio_encoder_short.clear();
    if(s.video_render_pixfmt.isEmpty())
      s.video_render_pixfmt = QStringLiteral("rgba");
    s.width = std::max(16, s.width);
    s.height = std::max(16, s.height);
    s.rate = std::max(1., s.rate);
    // An encoder this FFmpeg lacks: lossless, which every FFmpeg has
    if(!hasEncoder(s.video_encoder_short.toUtf8().constData()))
    {
      s.video_encoder_short = QStringLiteral("ffv1");
      s.video_converted_pixfmt = QStringLiteral("yuv420p");
      s.options.clear();
    }
    return s;
  }
  Gfx::LibavOutputSettings s;
  s.path = file;
  s.width = std::max(16, v.size.width());
  s.height = std::max(16, v.size.height());
  s.rate = std::max(1, v.fps);
  s.audio_channels = 0;
  s.video_render_pixfmt = QStringLiteral("rgba");

  auto codec = v.codec;
  const char* needed = codec == VideoSettings::H264   ? "libx264"
                       : codec == VideoSettings::H265 ? "libx265"
                       : codec == VideoSettings::Hap  ? "hap"
                       : codec == VideoSettings::ProRes ? "prores_ks"
                                                        : "ffv1";
  if(!hasEncoder(needed))
    codec = VideoSettings::Ffv1;

  switch(codec)
  {
    case VideoSettings::H264:
      s.muxer = QStringLiteral("matroska");
      s.video_encoder_short = QStringLiteral("libx264");
      s.video_converted_pixfmt = QStringLiteral("yuv420p");
      s.options = {{"preset", "veryfast"}, {"crf", "18"}};
      break;
    case VideoSettings::H265:
      s.muxer = QStringLiteral("matroska");
      s.video_encoder_short = QStringLiteral("libx265");
      s.video_converted_pixfmt = QStringLiteral("yuv420p");
      s.options = {{"preset", "fast"}, {"crf", "22"}};
      break;
    case VideoSettings::Hap:
      s.muxer = QStringLiteral("mov");
      s.video_encoder_short = QStringLiteral("hap");
      s.video_converted_pixfmt = QStringLiteral("rgba");
      s.options = {{"format", "hap"}};
      break;
    case VideoSettings::ProRes:
      s.muxer = QStringLiteral("mov");
      s.video_encoder_short = QStringLiteral("prores_ks");
      s.video_converted_pixfmt = QStringLiteral("yuv422p10le");
      s.options = {{"profile", "2"}};
      break;
    case VideoSettings::Ffv1:
      s.muxer = QStringLiteral("matroska");
      s.video_encoder_short = QStringLiteral("ffv1");
      s.video_converted_pixfmt = QStringLiteral("yuv420p");
      break;
  }
  return s;
}
#endif
}

bool TextureRecorder::canRecord(const Process::Port& port) noexcept
{
  return qobject_cast<const Gfx::TextureOutlet*>(&port)
         || qobject_cast<const Gfx::TextureInlet*>(&port);
}

ossia::gfx::port_index
TextureRecorder::producerOf(const Process::Port& port, const score::DocumentContext& doc)
{
  constexpr ossia::gfx::port_index none{-1, -1};
  if(auto* out = qobject_cast<const Gfx::TextureOutlet*>(&port))
    return out->graphicsPort();
  // An inlet: what its cables bring, else the device its address names
  for(const auto& path : port.cables())
    if(auto* cable = path.try_find(doc))
      if(auto* src = qobject_cast<Gfx::TextureOutlet*>(cable->source().try_find(doc)))
        return src->graphicsPort();
  const auto& addr = port.address().address;
  if(addr.device.isEmpty())
    return none;
  auto* devices = doc.findPlugin<Explorer::DeviceDocumentPlugin>();
  auto* dev = devices ? devices->list().findDevice(addr.device) : nullptr;
  auto* ossia_dev = dev ? dev->getDevice() : nullptr;
  if(!ossia_dev)
    return none;
  if(auto* node = Device::findNodeFromPath(addr.path, *ossia_dev))
    if(auto* p = dynamic_cast<Gfx::texture_source_parameter*>(node->get_parameter()))
      return p->source();
  return none;
}

TextureRecorder::TextureRecorder(
    const Process::Port& port, const score::DocumentContext& ctx, QString file,
    VideoSettings settings)
    : TextureRecorder{
          [p = QPointer<const Process::Port>{&port}, &ctx]() -> ossia::gfx::port_index {
    if(p)
      return producerOf(*p, ctx);
    return {-1, -1};
  }, ctx, std::move(file), settings}
{
}

TextureRecorder::TextureRecorder(
    Producer producer, const score::DocumentContext& ctx, QString file,
    VideoSettings settings)
    : m_producer{std::move(producer)}
    , m_ctx{ctx}
    , m_file{std::move(file)}
    , m_settings{settings}
{
  m_follow.setInterval(250);
  connect(&m_follow, &QTimer::timeout, this, &TextureRecorder::follow);
}

TextureRecorder::~TextureRecorder()
{
  stop();
}

#if SCORE_HAS_LIBAV
namespace
{
// The node is let go of by the render thread, some time after it is
// unregistered: it keeps the encoder it writes to alive until then. The
// holder is a base listed first, so that the encoder outlives the node part.
struct EncoderHolder
{
  std::shared_ptr<Gfx::LibavEncoder> held;
};
struct TakeEncoderNode final
    : EncoderHolder
    , Gfx::LibavEncoderNode
{
  TakeEncoderNode(
      const Gfx::LibavOutputSettings& set, std::shared_ptr<Gfx::LibavEncoder> enc)
      : EncoderHolder{std::move(enc)}
      , Gfx::LibavEncoderNode{set, *held, 0}
  {
  }
};
}
#endif

bool TextureRecorder::start()
{
#if SCORE_HAS_LIBAV
  auto* plug = m_ctx.findPlugin<Gfx::DocumentPlugin>();
  if(!m_producer || !plug)
    return false;
  const auto set = outputSettings(m_file, m_settings);
  m_encoder = std::make_shared<Gfx::LibavEncoder>(set);
  if(m_encoder->start() != 0)
  {
    m_encoder.reset();
    return false;
  }
  m_node = plug->context.register_preview_node(
      std::make_unique<TakeEncoderNode>(set, m_encoder));
  follow();
  m_follow.start();
  return true;
#else
  return false;
#endif
}

void TextureRecorder::follow()
{
  // The producer is made again when its process or its device is
  auto* plug = m_ctx.findPlugin<Gfx::DocumentPlugin>();
  if(!plug || m_node < 0)
    return;
  const auto src = m_producer();
  if(src == m_source)
    return;
  auto& gfx = plug->context;
  if(m_source.node >= 0)
    gfx.disconnect_preview_node(
        {m_source, {m_node, 0}, Process::CableType::ImmediateGlutton});
  m_source = src;
  if(m_source.node >= 0)
    gfx.connect_preview_node({m_source, {m_node, 0}, Process::CableType::ImmediateGlutton});
}

int64_t TextureRecorder::frames() const noexcept
{
#if SCORE_HAS_LIBAV
  if(!m_encoder)
    return m_frames;
  std::lock_guard _{m_encoder->m_muxMutex};
  if(m_encoder->video_stream_index >= 0
     && m_encoder->video_stream_index < int(m_encoder->streams.size()))
    return m_encoder->streams[m_encoder->video_stream_index].next_pts;
#endif
  return m_frames;
}

void TextureRecorder::stop()
{
  m_follow.stop();
#if SCORE_HAS_LIBAV
  if(!m_encoder)
    return;
  m_frames = frames();
  if(auto* plug = m_ctx.findPlugin<Gfx::DocumentPlugin>())
  {
    auto& gfx = plug->context;
    if(m_source.node >= 0)
      gfx.disconnect_preview_node(
          {m_source, {m_node, 0}, Process::CableType::ImmediateGlutton});
    if(m_node >= 0)
      gfx.unregister_preview_node(m_node);
  }
  m_node = -1;
  m_source = {-1, -1};
  // Frames still being rendered find the file closed: the encoder checks
  // under its lock. The node keeps it alive until the render thread lets go.
  m_encoder->stop();
  m_encoder.reset();
#endif
}
}
