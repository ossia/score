#include "TextureRecorder.hpp"

#include <Media/Libav.hpp>

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/GfxContext.hpp>
#include <Gfx/Libav/LibavEncoder.hpp>
#include <Gfx/TexturePort.hpp>
#include <Gfx/Widgets/TexturePreview.hpp>

#include <Process/Dataflow/Cable.hpp>

#include <Device/Protocol/DeviceInterface.hpp>

#include <Explorer/DeviceList.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Gfx/GfxInputDevice.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/model/path/PathSerialization.hpp>

#include <QDateTime>
#include <QElapsedTimer>

#if SCORE_HAS_LIBAV
extern "C" {
#include <libavcodec/avcodec.h>
}
#endif

namespace Recording
{
namespace
{
//! The largest side the preview path reads back
constexpr int max_side = Gfx::GfxContext::max_preview_size;
//! Frames waiting for the encoder, at most (a second of 1024 x 1024 RGBA is
//! 120 MB): past that they are dropped
constexpr std::size_t max_queued = 30;

#if SCORE_HAS_LIBAV
Gfx::LibavOutputSettings
videoSettings(const QString& file, QSize size, int fps, bool lossless)
{
  Gfx::LibavOutputSettings s;
  s.path = file;
  s.width = size.width();
  s.height = size.height();
  s.rate = fps;
  s.muxer = QStringLiteral("matroska");
  s.video_render_pixfmt = QStringLiteral("rgba");
  // H.264 when it is there, else lossless FFV1, which every FFmpeg has
  if(!lossless && avcodec_find_encoder_by_name("libx264"))
  {
    s.video_encoder_short = QStringLiteral("libx264");
    s.video_converted_pixfmt = QStringLiteral("yuv420p");
    s.options = {{"preset", "veryfast"}, {"crf", "18"}};
  }
  else
  {
    s.video_encoder_short = QStringLiteral("ffv1");
    s.video_converted_pixfmt = QStringLiteral("yuv420p");
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
    const Process::Port& port, const score::DocumentContext& ctx, QString file, int fps,
    bool lossless)
    : TextureRecorder{
          [p = QPointer<const Process::Port>{&port}, &ctx]() -> ossia::gfx::port_index {
    if(p)
      return producerOf(*p, ctx);
    return {-1, -1};
  }, ctx, std::move(file), fps, lossless}
{
}

TextureRecorder::TextureRecorder(
    Producer producer, const score::DocumentContext& ctx, QString file, int fps,
    bool lossless)
    : m_producer{std::move(producer)}
    , m_ctx{ctx}
    , m_file{std::move(file)}
    , m_fps{std::max(1, fps)}
    , m_lossless{lossless}
{
  m_timer.setTimerType(Qt::PreciseTimer);
  m_timer.setInterval(1000 / m_fps);
  connect(&m_timer, &QTimer::timeout, this, &TextureRecorder::take);
}

TextureRecorder::~TextureRecorder()
{
  stop();
}

bool TextureRecorder::start()
{
#if !SCORE_HAS_LIBAV
  return false;
#endif
  auto* plug = m_ctx.findPlugin<Gfx::DocumentPlugin>();
  if(!m_producer || !plug)
    return false;
  m_preview = std::make_unique<Gfx::TexturePreview>();
  m_preview->attach(&plug->context, m_producer(), QSize{max_side, max_side});
  if(!m_preview->attached())
    return false;
  m_startMs = QDateTime::currentMSecsSinceEpoch();
  m_timer.start();
  return true;
}

void TextureRecorder::take()
{
  if(!m_preview)
    return;
  // The producer changes when its process or device is made again
  m_preview->setProducer(m_producer());

  QImage img = m_preview->image();
  if(img.isNull())
    return;
  img = img.convertToFormat(QImage::Format_RGBA8888);

  if(!m_frameSize.isValid())
  {
    // The first frame read back: the video starts now, at its size
    m_frameSize = img.size();
    m_startMs = QDateTime::currentMSecsSinceEpoch();
    m_done = false;
    m_thread = std::thread{[this] { encodeLoop(); }};
  }
  else if(img.size() != m_frameSize)
  {
    img = img.scaled(m_frameSize);
  }

  // As many frames as the time that passed asks for: repeated when the GUI
  // was late, none when it was early
  const qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - m_startMs;
  const int64_t due = elapsed * m_fps / 1000 + 1;
  std::lock_guard _{m_lock};
  while(m_taken < due)
  {
    if(m_queue.size() < max_queued)
      m_queue.push_back(img);
    m_taken++;
  }
  m_cond.notify_one();
}

void TextureRecorder::encodeLoop()
{
#if SCORE_HAS_LIBAV
  Gfx::LibavEncoder enc{videoSettings(m_file, m_frameSize, m_fps, m_lossless)};
  if(enc.start() != 0)
  {
    std::unique_lock lk{m_lock};
    m_queue.clear();
    return;
  }
  for(;;)
  {
    QImage img;
    {
      std::unique_lock lk{m_lock};
      m_cond.wait(lk, [this] { return m_done || !m_queue.empty(); });
      if(m_queue.empty() && m_done)
        break;
      img = std::move(m_queue.front());
      m_queue.pop_front();
    }
    if(enc.add_frame(img.constBits(), AV_PIX_FMT_RGBA, img.width(), img.height()) == 0)
      m_written++;
  }
  enc.stop();
#endif
}

void TextureRecorder::stop()
{
  m_timer.stop();
  if(m_preview)
  {
    m_preview->detach();
    m_preview.reset();
  }
  {
    std::lock_guard _{m_lock};
    m_done = true;
  }
  m_cond.notify_one();
  if(m_thread.joinable())
    m_thread.join();
}
}
