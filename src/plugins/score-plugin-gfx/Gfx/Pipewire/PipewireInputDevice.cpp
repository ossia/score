// SPDX-License-Identifier: GPL-3.0-or-later
//
// PipeWire video INPUT device for score.
//
// Connects to a PipeWire node providing video frames and surfaces them as
// AVFrames into score's score::gfx pipeline via the existing
// `Gfx::video_texture_input_protocol` plumbing (the same path used by
// libav / V4L2 / camera inputs).
//
// Design follows pipewire's upstream `src/examples/video-play.c` for the
// consumer shape: stream-with-state-callbacks model, format negotiated in
// on_stream_param_changed via spa_format_video_raw_parse, frames pulled
// in on_stream_process. The score-side adaptation:
//
//   - Pipewire main loop runs on a dedicated std::thread (per-stream,
//     not shared) — keeps the streaming thread isolated from any other
//     pipewire consumers in the process.
//   - Frames land in an AVFrame pool guarded by a mutex; the score
//     renderer pulls via dequeue_frame().
//   - Format renegotiation rebuilds the AVFrame pool to match the new
//     geometry / pixel format.
//
// Build is gated on OSSIA_ENABLE_PIPEWIRE (see CMakeLists); the factory
// is only registered in score_plugin_gfx.cpp when the dep is present.

#include "PipewireInputDevice.hpp"

#include "PipewireFormats.hpp"

#include <Gfx/Graph/interop/DrmPrimeWrap.hpp>

#include <cmath>

#include <libremidi/backends/linux/pipewire/context.hpp>
#include <libremidi/backends/linux/pipewire/format.hpp>
#include <libremidi/backends/linux/pipewire/loader.hpp>
#include <libremidi/backends/linux/pipewire/subscription.hpp>
#include <libremidi/backends/linux/pipewire/types.hpp>

#include <Device/Protocol/DeviceSettings.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/GfxDevice.hpp>
#include <Gfx/GfxInputDevice.hpp>
#include <Gfx/SharedInputSettings.hpp>
#include <Video/ExternalInput.hpp>

#include <score/document/DocumentContext.hpp>

#include <QComboBox>
#include <QDebug>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QObject>
#include <QRegularExpression>
#include <QSpinBox>

// PipeWire / SPA headers — included for types, macros, and the
// header-only SPA pod-builder inline functions. No `pw_*` function
// is called directly; every extern symbol is routed through
// `libremidi::pipewire::load()` (the shared loader). Including these
// headers does NOT create a DT_NEEDED for libpipewire as long as the
// only references are types/macros/inlines.
#include <pipewire/pipewire.h>
#include <spa/param/latency-utils.h>
#include <spa/param/video/format-utils.h>
#include <spa/utils/result.h>

#include <wobjectimpl.h>

extern "C" {
#include <libavutil/buffer.h>
#include <libavutil/frame.h>
#include <libavutil/hwcontext_drm.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
}

#include <atomic>
#include <cstring>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <span>
#include <vector>

namespace Gfx::PipeWire
{

namespace
{

// Default AVFrame pool size — matches upstream pipewire examples'
// MAX_BUFFERS lower bound and keeps memory reasonable at 1080p.
constexpr int kDefaultPoolSize = 8;

// Thin wrappers around formats::* — kept as named functions so the
// rest of the file reads naturally. The Tag-based helper handles the
// full HDR / planar / packed matrix.
AVPixelFormat spaToAvPixFmt(uint32_t spaFmt) noexcept
{
  return formats::toAvPixFmt(formats::tagFromSpa(spaFmt));
}

spa_video_format avToSpaPixFmt(AVPixelFormat fmt) noexcept
{
  return formats::toSpa(formats::tagFromAvPixFmt(fmt));
}

/** DMA-BUF modifiers this consumer advertises in EnumFormat — the seam for
 *  GPU-capability-driven negotiation.
 *
 *  Baseline: LINEAR + INVALID (portable; every import path handles LINEAR,
 *  INVALID lets implicit-modifier producers connect). The interop layer
 *  already has the real probes — `score::gfx::vkinterop::
 *  supported_dmabuf_modifiers(VulkanCtx, VkFormat)` and
 *  `EglDmaBufImporter::supportedModifiers(drm_fourcc)` — but at
 *  InputStream::start() time no renderer (and thus no VkDevice / GL
 *  context) exists yet: the stream is created by the device layer before
 *  any RenderList. Wiring them in needs either a deferred renegotiation
 *  (pw_stream_update_params once the first renderer attaches) or a
 *  graphics-context handle plumbed into the device settings; until then
 *  every tiled producer fixates down to LINEAR. The tag parameter is
 *  what the eventual probe needs to pick the right VkFormat/fourcc. */
std::span<const std::uint64_t> consumerDmabufModifiers(formats::Tag) noexcept
{
  static constexpr std::uint64_t mods[]{
      0ULL,                 // DRM_FORMAT_MOD_LINEAR
      0x00FFFFFFFFFFFFFFULL // DRM_FORMAT_MOD_INVALID
  };
  return {mods, std::size(mods)};
}

// DMA-BUF handling:
//
// Two paths coexist based on the negotiated SPA format:
//
//   - **Packed RGB** (BGRA8 / RGBA8 / RGB10A2 / RGBA16F) DMA-BUF →
//     wrapped as AV_PIX_FMT_DRM_PRIME AVFrame with an
//     AVDRMFrameDescriptor in data[0]. Score's DRMPrimeDecoder
//     (registered in GPUVideoDecoderFactory) imports the FD as a
//     Vulkan VkImage via VK_EXT_image_drm_format_modifier and re-
//     wraps as a QRhiTexture — true zero-copy.
//
//   - **Planar YUV** (NV12 / P010 / I420) DMA-BUF → consumed via the
//     mmap'd `datas[0].data` pointer (`PW_STREAM_FLAG_MAP_BUFFERS`
//     handles the mmap), then memcpy'd plane-by-plane into a pool
//     AVFrame. The renderer's existing NV12/P010/YUV420 decoders
//     handle these. Multi-plane DMA-BUF import is a follow-up that
//     needs per-plane VkImage import + a branching YUV→RGB shader.
//
// The choice is per-frame based on `buf->datas[0].type` AND the SPA-
// negotiated format. If something unexpected arrives (e.g. producer
// sends DmaBuf with a fourcc DRMPrimeDecoder doesn't recognise), we
// fall back to the memcpy path automatically.

// Published pw_stream pointer, shared between the stream owner and every
// in-flight frame's release context. The owner nulls it on the pipewire
// loop (before pw_stream_destroy) so a frame released after stop() sees
// null and no-ops instead of queueing a buffer on a destroyed stream.
// The process-wide pipewire context outlives any one stream, so the
// weak_ctx guard alone is not enough to detect stream teardown.
struct PwStreamToken
{
  std::atomic<pw_stream*> stream{nullptr};
};

// Holds the per-frame context for the deferred queue-back path.
struct PwBufferReleaseCtx
{
  std::weak_ptr<libremidi::pipewire::context> weak_ctx;
  std::shared_ptr<PwStreamToken> token;
  pw_buffer* buffer{};
  AVDRMFrameDescriptor* desc{};
};

// Runs on whichever thread is unrefing the AVFrame (typically score's
// render thread). Synchronously hops to the pipewire loop thread to
// queue-back the pipewire buffer, then cleans up the descriptor +
// release context. invoke_sync waits for the queued callback to run
// before returning, so the `delete ctx` below happens AFTER pipewire
// has consumed ctx — not before, which would be the heap-use-after-
// free bug.
extern "C" void score_pw_release_avframe(void* opaque, uint8_t* /*data*/)
{
  auto* ctx = static_cast<PwBufferReleaseCtx*>(opaque);
  // The token's stream pointer is read on the pipewire loop thread,
  // where the owner also nulls it before destroying the stream — so
  // either we queue the buffer back on a live stream, or we see null
  // and only free our descriptor.
  if(auto shared = ctx->weak_ctx.lock(); shared && ctx->token && ctx->buffer)
  {
    auto& pw = libremidi::pipewire::load();
    pw_buffer* buf = ctx->buffer;
    const auto& token = ctx->token;
    shared->invoke_sync([&] {
      if(pw_stream* stream = token->stream.load(std::memory_order_relaxed))
        if(pw.stream_queue_buffer)
          pw.stream_queue_buffer(stream, buf);
    });
  }
  if(ctx->desc)
    std::free(ctx->desc);
  delete ctx;
}

/* Build an AVDRMFrameDescriptor for a pipewire DMA-BUF buffer.
 *
 * Handles three layout shapes the producer can deliver:
 *   - Single-plane packed (BGRA, RGBA, RGB10A2, RGBA16F) →
 *     1 object, 1 layer, 1 plane. The layer's fourcc is the packed
 *     format (DRM_FORMAT_ARGB8888, etc.).
 *   - Single-object multi-plane (NV12, P010, I420 in one allocation
 *     with per-plane offsets) → 1 object, 1 layer with N planes.
 *     Common from pipewire when the producer uses one DMA-BUF for
 *     all planes (typical for screencast portal, libcamera).
 *   - Multi-object multi-plane (rare: separate FDs per plane) →
 *     N objects, N layers each with 1 plane. Producer with split-
 *     buffer hardware.
 *
 * Returns nullptr if the SPA format isn't supported (caller falls
 * back to mmap+memcpy). */
AVFrame* wrap_dmabuf_as_drm_prime(
    const std::shared_ptr<libremidi::pipewire::context>& shared,
    const std::shared_ptr<PwStreamToken>& token, pw_buffer* b, int width,
    int height, uint64_t modifier, formats::Tag tag)
{
  spa_buffer* buf = b->buffer;
  if(!buf || buf->n_datas < 1)
    return nullptr;

  const uint32_t fourcc = formats::toDrmFourcc(tag);
  if(fourcc == 0)
    return nullptr;

  // Adapt the spa_data blocks to the protocol-agnostic span shape and let
  // the shared interop helper (Gfx/Graph/interop/DrmPrimeWrap.hpp) build the
  // descriptor + frame; plane layout is derived from the DRM fourcc there.
  // Only the release plumbing (queue the pw_buffer back on the loop thread)
  // stays pipewire-specific.
  namespace interop = score::gfx::interop;
  interop::DrmPlaneSpan blocks[AV_DRM_MAX_PLANES];
  const int n = std::min(int(buf->n_datas), int(AV_DRM_MAX_PLANES));
  for(int i = 0; i < n; ++i)
  {
    blocks[i].fd = int(buf->datas[i].fd);
    blocks[i].maxsize = buf->datas[i].maxsize;
    blocks[i].stride = buf->datas[i].chunk ? buf->datas[i].chunk->stride : 0;
    blocks[i].offset = buf->datas[i].chunk ? buf->datas[i].chunk->offset : 0;
  }
  auto* desc
      = interop::buildDrmDescriptor(blocks, n, fourcc, modifier, width, height);
  if(!desc)
    return nullptr;

  auto* ctx = new PwBufferReleaseCtx{
      std::weak_ptr<libremidi::pipewire::context>{shared}, token, b, desc};
  AVFrame* f = interop::wrapDescriptorAsDrmPrime(
      desc, width, height, &score_pw_release_avframe, ctx);
  if(!f)
  {
    delete ctx; // desc already freed by the helper
    return nullptr;
  }
  return f;
}

} // namespace

// ============================================================================
// InputStream: per-connection PipeWire consumer with AVFrame pool
// ============================================================================

class InputStream final : public ::Video::ExternalInput
{
public:
  explicit InputStream(const QString& path) noexcept;
  ~InputStream() noexcept;

  bool start() noexcept override;
  void stop() noexcept override;

  AVFrame* dequeue_frame() noexcept override;
  void release_frame(AVFrame* frame) noexcept override;

  bool notifyFormatChange(
      int new_w, int new_h, AVPixelFormat new_pixfmt) noexcept override;

  int64_t duration() const noexcept { return 0; }
  void seek(int64_t /*flicks*/) noexcept { }

  double fps() const noexcept { return m_fps; }
  bool realTime() const noexcept { return true; }
  double flicks_per_dts() const noexcept { return 1.; }

private:
  struct PipeWireData
  {
    // Process-wide shared connection. Acquired lazily in start(). The
    // shared context owns the thread_loop, pw_context, pw_core and
    // registry — we just attach a pw_stream to its loop.
    std::shared_ptr<libremidi::pipewire::context> shared;
    pw_stream* stream{nullptr};
    // Handed to every DRM_PRIME frame's release context; see PwStreamToken.
    std::shared_ptr<PwStreamToken> token = std::make_shared<PwStreamToken>();
    spa_hook stream_listener{};
    spa_video_info_raw format{};

    PipeWireData() = default;
    ~PipeWireData() { cleanup(); }

    // Must be called with the thread_loop unlocked. Internally takes
    // the lock to destroy the stream, then drops the shared_ptr.
    void cleanup()
    {
      if(stream && shared)
      {
        auto& pw = libremidi::pipewire::load();
        shared->with_lock([&] {
          // Invalidate in-flight frame releases before the stream dies;
          // both run on the loop thread, so this is race-free.
          token->stream.store(nullptr, std::memory_order_relaxed);
          if(pw.stream_destroy)
            pw.stream_destroy(stream);
          stream = nullptr;
        });
        // Frames released after this point must not queue into a future
        // stream: they keep the old token, we start over with a new one.
        token = std::make_shared<PwStreamToken>();
      }
      shared.reset();
    }
  };

  void init_frames();
  void cleanup_frames();

  static void on_stream_state_changed(
      void* data, enum pw_stream_state old, enum pw_stream_state state,
      const char* error);
  static void
  on_stream_param_changed(void* data, uint32_t id, const struct spa_pod* param);
  static void on_stream_process(void* data);
  static const struct pw_stream_events stream_events;

  QString m_path;
  std::unique_ptr<PipeWireData> m_data;
  std::atomic<bool> m_running{false};

  // Registry subscriptions for daemon-level events. Live for the
  // duration of the stream; reset in stop() before the shared context
  // ref is dropped (otherwise subscription RAII would deadlock on a
  // dead context).
  std::vector<libremidi::pipewire::subscription> m_pw_subs;
  // Target node id (resolved at start time when a non-empty node name
  // was supplied in the URL). 0 means "unknown / Default". When
  // on_node_removed reports our target id, we set m_running=false so
  // the next dequeue exits cleanly.
  std::uint32_t m_target_node_id{0};

  // Negotiation helper. Holds the format/size/modifier-list state
  // for the two-pass DMA-BUF fixation handshake (and SHM fallback).
  libremidi::pipewire::format_negotiation m_neg;

  int m_width{1920};
  int m_height{1080};
  double m_fps{30.0};
  AVPixelFormat m_pixelFormat{AV_PIX_FMT_RGB24};
  // Requested wire format from the URL. Kept as the Tag (not just the
  // AVPixelFormat) because the Tag→AV mapping is lossy: YV12 and I420
  // both publish AVFrames as YUV420P, but negotiate different SPA
  // formats and need a U/V plane swap on copy.
  formats::Tag m_formatTag{formats::Tag::RGB24};
  // When the producer chose DMA-BUF allocation (latched on first
  // DmaBuf frame), m_pixelFormat above transitions to AV_PIX_FMT_DRM_PRIME
  // and m_sw_format below tracks the underlying SW pixel format
  // (the SPA-negotiated one — BGRA8, NV12, P010, …). DRMPrimeDecoder
  // reads `hwaccel_sw_format` from the base VideoMetadata to pick
  // the right shader.
  AVPixelFormat m_sw_format{AV_PIX_FMT_NONE};
  bool m_drm_prime_mode{false};

  std::mutex m_frameMutex;
  std::vector<AVFrame*> m_availableFrames;
  std::vector<AVFrame*> m_usedFrames;

public:
  // Testability: what the last format negotiation settled on.
  // 0 = none yet, 1 = shm, 2 = dmabuf. Read by
  // pipewireInputNegotiatedTransport (harness transport-truth column).
  std::atomic<int> m_negotiatedKind{0};
};

const struct pw_stream_events InputStream::stream_events = []() {
  pw_stream_events e{};
  e.version = PW_VERSION_STREAM_EVENTS;
  e.state_changed = on_stream_state_changed;
  e.param_changed = on_stream_param_changed;
  e.process = on_stream_process;
  return e;
}();

InputStream::InputStream(const QString& path) noexcept
    : m_path(path)
    , m_data(std::make_unique<PipeWireData>())
{
  // Parse the score-side URL convention: pipewire://[node]?key=value&...
  static const QRegularExpression urlRe("pipewire://([^?]*)(?:\\?(.*))?");
  auto m = urlRe.match(path);
  if(m.hasMatch())
  {
    const QString params = m.captured(2);
    if(!params.isEmpty())
    {
      static const QRegularExpression paramRe("([^=&]+)=([^&]+)");
      auto it = paramRe.globalMatch(params);
      while(it.hasNext())
      {
        const auto pm = it.next();
        const QString k = pm.captured(1), v = pm.captured(2);
        if(k == "width")
          m_width = v.toInt();
        else if(k == "height")
          m_height = v.toInt();
        else if(k == "fps")
          m_fps = v.toDouble();
        else if(k == "format")
        {
          // Use the shared Tag-based helper so the URL accepts the
          // same name set as the settings widget and the SPA enum.
          const auto tag = formats::tagFromString(v);
          const auto av = formats::toAvPixFmt(tag);
          if(av != AV_PIX_FMT_NONE)
          {
            m_formatTag = tag;
            m_pixelFormat = av;
          }
          else
          {
            m_formatTag = formats::Tag::RGB24;
            m_pixelFormat = AV_PIX_FMT_RGB24;
          }
        }
      }
    }
  }

  // Seed the inherited VideoInterface metadata from URL defaults so the
  // renderer-rebuild detection starts from a coherent state. A
  // subsequent on_stream_param_changed will refine when the server
  // negotiates.
  ::Video::ExternalInput::notifyFormatChange(m_width, m_height, m_pixelFormat);

  init_frames();
}

InputStream::~InputStream() noexcept
{
  stop();
  cleanup_frames();
}

void InputStream::init_frames()
{
  // The pool is for sysmem (memcpy) frames only — DRM_PRIME frames
  // are heap-allocated per-call by wrap_dmabuf_as_drm_prime and
  // freed via av_frame_free in release_frame. Skip pool allocation
  // when we're committed to DRM_PRIME so we don't waste memory on
  // frames that'll never be used.
  if(m_pixelFormat == AV_PIX_FMT_DRM_PRIME)
    return;
  // Runs on the pipewire loop thread (renegotiation) concurrently with
  // the render thread's release_frame; the pool needs the lock.
  std::lock_guard<std::mutex> lock(m_frameMutex);
  for(int i = 0; i < kDefaultPoolSize; ++i)
  {
    AVFrame* f = av_frame_alloc();
    if(!f)
      continue;
    f->format = m_pixelFormat;
    f->width = m_width;
    f->height = m_height;
    if(av_frame_get_buffer(f, 32) < 0)
    {
      av_frame_free(&f);
      continue;
    }
    m_availableFrames.push_back(f);
  }
}

void InputStream::cleanup_frames()
{
  std::lock_guard<std::mutex> lock(m_frameMutex);
  for(auto* f : m_availableFrames)
    av_frame_free(&f);
  for(auto* f : m_usedFrames)
    av_frame_free(&f);
  m_availableFrames.clear();
  m_usedFrames.clear();
}

bool InputStream::start() noexcept
{
  if(m_running.exchange(true))
    return true;

  // If the previous stream ended via on_node_removed (m_running was
  // cleared without stop() running), it is still alive here — tear it
  // down before creating a new one, or its listener keeps firing into
  // this object alongside the new stream's.
  m_pw_subs.clear();
  m_data->cleanup();

  auto& pw = libremidi::pipewire::load();
  if(!pw.stream_available)
  {
    qWarning()
        << "PipeWire input start: libpipewire-0.3 stream API not available";
    m_running = false;
    return false;
  }

  try
  {
    // Acquire the process-wide shared context. Same connection as the
    // audio engine and libremidi MIDI backends — one pipewire socket
    // per process. Rebuild on broken state.
    m_data->shared = libremidi::pipewire::shared_context();
    if(m_data->shared
       && m_data->shared->state()
              == libremidi::pipewire::connection_state::broken)
    {
      m_data->shared->reconnect();
    }
    if(!m_data->shared || !m_data->shared->ok())
      throw std::runtime_error("Failed to acquire pipewire shared context");

    auto* props = pw.properties_new(
        PW_KEY_MEDIA_TYPE, "Video", PW_KEY_MEDIA_CATEGORY, "Capture",
        PW_KEY_MEDIA_ROLE, "Camera", nullptr);

    // Parse the node target out of the URL again (start may run after
    // the path was reset).
    static const QRegularExpression nodeRe("pipewire://([^?]*)");
    auto match = nodeRe.match(m_path);
    QString targetName;
    if(match.hasMatch() && !match.captured(1).isEmpty())
    {
      targetName = match.captured(1);
      // node.target is deprecated since 0.3.64; WirePlumber 0.5+ only
      // honors target.object (name or serial). Set both so by-name
      // source selection works on every session manager generation.
      pw.properties_set(
          props, PW_KEY_NODE_TARGET, targetName.toUtf8().constData());
      pw.properties_set(
          props, "target.object", targetName.toUtf8().constData());
    }

    // Resolve target node id from the snapshot so on_node_removed
    // can identify when our specific source disappears (revoked
    // permission, USB camera unplugged, monitor dropped, etc.).
    m_target_node_id = 0;
    if(!targetName.isEmpty())
    {
      const auto snap = m_data->shared->snapshot();
      if(auto* n = snap.find_by_name(targetName.toStdString()))
        m_target_node_id = n->id;
    }

    // Subscribe to daemon-level events for live recovery signalling.
    // Subscriptions are RAII tokens; held by m_pw_subs and cleared
    // before the shared context ref is dropped in stop().
    m_pw_subs.clear();
    m_pw_subs.push_back(m_data->shared->on_state_changed(
        [](libremidi::pipewire::connection_state s) {
      using cs = libremidi::pipewire::connection_state;
      const char* name = (s == cs::connected)    ? "connected"
                         : (s == cs::connecting) ? "connecting"
                         : (s == cs::broken)     ? "broken"
                                                 : "disconnected";
      qDebug() << "PipeWire input: pipewire state ->" << name;
    }));
    m_pw_subs.push_back(m_data->shared->on_node_removed(
        [this](std::uint32_t removed_id) {
      if(m_target_node_id != 0 && removed_id == m_target_node_id)
      {
        qDebug()
            << "PipeWire input: target node disappeared; stopping";
        m_running.store(false, std::memory_order_release);
      }
    }));

    // Stream creation, listener install and connect all touch the
    // proxy layer; they must happen with the thread_loop lock held.
    bool ok = false;
    m_data->shared->with_lock([&] {
      m_data->stream = pw.stream_new(
          m_data->shared->pw_core_ptr(), "score-input", props);
      if(!m_data->stream)
        return;
      m_data->token->stream.store(m_data->stream, std::memory_order_relaxed);
      pw.stream_add_listener(
          m_data->stream, &m_data->stream_listener, &stream_events, this);
      ok = true;
    });
    if(!m_data->stream)
      throw std::runtime_error("Failed to create pipewire stream");
    (void)ok;

    // Modifier-aware EnumFormat building via the shared format
    // negotiation helper. SCORE_PIPEWIRE_FORCE_SHM=1 disables the
    // DMA-BUF path entirely (useful for debugging modifier mismatches
    // and for headless test rigs without a working GPU import path).
    const bool force_shm
        = qEnvironmentVariableIntValue("SCORE_PIPEWIRE_FORCE_SHM") == 1;

    libremidi::pipewire::format_negotiation neg;
    neg.set_size(m_width, m_height);
    // Keep fractional rates exact: 29.97→29970/1000, 59.94→59940/1000.
    // A plain (int) cast truncated NTSC rates to 29/1 and 59/1.
    const int fps_num = int(std::lround(m_fps * 1000.0));
    neg.set_framerate(fps_num, 1000, fps_num);
    // Negotiate the URL's Tag, not the AVPixelFormat: the Tag→AV mapping
    // is lossy (YV12 publishes as YUV420P) and would lose the requested
    // wire format.
    neg.set_video_format(formats::toSpa(m_formatTag));
    neg.set_shm_fallback(true);
    if(!force_shm)
      neg.set_dmabuf_modifiers(consumerDmabufModifiers(m_formatTag));
    m_neg = std::move(neg);

    uint8_t buffer[2048];
    spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    const auto params = m_neg.build_connect_params(b, PW_DIRECTION_INPUT);
    if(params.empty())
      throw std::runtime_error("format_negotiation produced no params");

    // stream_connect also goes through the proxy layer — lock.
    int connect_ret = 0;
    m_data->shared->with_lock([&] {
      connect_ret = pw.stream_connect(
          m_data->stream, PW_DIRECTION_INPUT, PW_ID_ANY,
          (enum pw_stream_flags)(
              PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS),
          const_cast<const spa_pod**>(params.data()),
          static_cast<std::uint32_t>(params.size()));
    });
    if(connect_ret < 0)
      throw std::runtime_error("pw_stream_connect failed");

    // No std::thread to spawn — the shared context's thread_loop is
    // already running and will dispatch our stream callbacks.
    return true;
  }
  catch(const std::exception& e)
  {
    qDebug() << "PipeWire input start failed:" << e.what();
    m_running = false;
    m_data->cleanup();
    return false;
  }
}

void InputStream::stop() noexcept
{
  // No early-return on m_running: on_node_removed clears the flag when
  // the producer vanishes, but the stream and subscriptions still exist
  // and must be torn down here (both are safe to tear down twice).
  m_running.store(false, std::memory_order_release);
  // Drop subscriptions before the shared context ref so each
  // subscription RAII goes through the live context (the subscription
  // dtor calls back into context::unsubscribe).
  m_pw_subs.clear();
  // The shared context's thread_loop keeps running; we just unhook
  // and destroy our stream. cleanup() takes the lock internally.
  m_data->cleanup();

  // Reset the DMA-BUF latch: m_drm_prime_mode/m_sw_format are decided on
  // the FIRST DmaBuf frame of a stream. A restarted stream (new producer,
  // possibly a different format) must re-latch — keeping the stale values
  // would wrap the new producer's buffers with the old fourcc/sw-format.
  m_drm_prime_mode = false;
  m_sw_format = AV_PIX_FMT_NONE;
  m_negotiatedKind.store(0, std::memory_order_relaxed);
}

AVFrame* InputStream::dequeue_frame() noexcept
{
  std::lock_guard<std::mutex> lock(m_frameMutex);
  if(m_usedFrames.empty())
    return nullptr;
  AVFrame* f = m_usedFrames.front();
  m_usedFrames.erase(m_usedFrames.begin());
  return f;
}

void InputStream::release_frame(AVFrame* f) noexcept
{
  if(!f)
    return;
  // DRM_PRIME frames are heap-allocated per on_stream_process call
  // (single-plane packed-RGB DMA-BUF wrap). Their buf[0] callback
  // queues the pipewire buffer back synchronously inside av_frame_free.
  if(f->format == AV_PIX_FMT_DRM_PRIME)
  {
    av_frame_free(&f);
    return;
  }
  // Sysmem-path frames go back to the reuse pool.
  std::lock_guard<std::mutex> lock(m_frameMutex);
  m_availableFrames.push_back(f);
}

bool InputStream::notifyFormatChange(
    int new_w, int new_h, AVPixelFormat new_pixfmt) noexcept
{
  // `width`, `height`, `pixel_format` are public data members on the
  // VideoInterface base; comparing against them tells us whether the
  // server-negotiated state has actually changed.
  if(new_w == width && new_h == height && new_pixfmt == pixel_format)
    return false;

  m_width = new_w;
  m_height = new_h;
  m_pixelFormat = new_pixfmt;

  cleanup_frames();
  init_frames();

  return ::Video::ExternalInput::notifyFormatChange(new_w, new_h, new_pixfmt);
}

void InputStream::on_stream_state_changed(
    void* data, enum pw_stream_state /*old*/, enum pw_stream_state state,
    const char* error)
{
  auto* self = static_cast<InputStream*>(data);
  (void)self;
  switch(state)
  {
    case PW_STREAM_STATE_ERROR:
      qDebug() << "PipeWire stream error:" << (error ? error : "(unknown)");
      break;
    case PW_STREAM_STATE_UNCONNECTED:
      qDebug() << "PipeWire stream unconnected";
      break;
    case PW_STREAM_STATE_PAUSED:
      qDebug() << "PipeWire stream paused";
      break;
    case PW_STREAM_STATE_STREAMING:
      qDebug() << "PipeWire stream streaming";
      break;
    default:
      break;
  }
}

void InputStream::on_stream_param_changed(
    void* data, uint32_t id, const struct spa_pod* param)
{
  auto* self = static_cast<InputStream*>(data);
  using ng = libremidi::pipewire::format_negotiation;
  const auto pc = self->m_neg.on_param_changed(id, param);
  if(pc.kind == ng::result::unrelated)
    return;

  // Two-pass DMA-BUF: producer asked us to fixate. Re-announce the
  // EnumFormat with a single modifier chosen from the candidate list.
  // The producer responds with a fresh Format event that hits the
  // dmabuf_fixated branch below.
  if(pc.kind == ng::result::needs_fixation)
  {
    if(pc.candidate_modifiers.empty())
      return;
    // Baseline policy: prefer LINEAR when offered, otherwise take the
    // producer's first choice. Follow-up: probe each candidate against
    // the GPU import path and pick the cheapest supported tiled modifier
    // instead of always LINEAR.
    std::uint64_t chosen = pc.candidate_modifiers.front();
    for(auto m : pc.candidate_modifiers)
      if(m == 0)
      {
        chosen = 0;
        break;
      }
    uint8_t buf[2048];
    spa_pod_builder b = SPA_POD_BUILDER_INIT(buf, sizeof(buf));
    const auto params = self->m_neg.build_fixation_params(b, chosen);
    libremidi::pipewire::load().stream_update_params(
        self->m_data->stream,
        const_cast<const spa_pod**>(params.data()),
        static_cast<std::uint32_t>(params.size()));
    return;
  }

  // Negotiation done — either dmabuf_fixated or shm_fallback.
  self->m_data->format.format = pc.format;
  self->m_data->format.size.width = static_cast<std::uint32_t>(pc.width);
  self->m_data->format.size.height = static_cast<std::uint32_t>(pc.height);
  if(pc.kind == ng::result::dmabuf_fixated)
    self->m_data->format.modifier = pc.chosen_modifier;
  else
    self->m_data->format.modifier = 0;

  const int new_w = pc.width;
  const int new_h = pc.height;
  if(new_w <= 0 || new_h <= 0)
    return;

  const AVPixelFormat parsed = spaToAvPixFmt(pc.format);
  const AVPixelFormat new_pf
      = (parsed != AV_PIX_FMT_NONE) ? parsed : self->m_pixelFormat;

  qDebug() << "PipeWire negotiated:" << new_w << "x" << new_h << "fmt"
           << av_get_pix_fmt_name(new_pf) << "mod" << pc.chosen_modifier
           << "kind" << (pc.kind == ng::result::dmabuf_fixated
                             ? "dmabuf" : "shm");
  self->m_negotiatedKind.store(
      pc.kind == ng::result::dmabuf_fixated ? 2 : 1,
      std::memory_order_relaxed);

  // A (re)negotiated format invalidates the DMA-BUF latch: the next DmaBuf
  // frame re-latches m_sw_format from the fresh negotiation instead of
  // wrapping new buffers with the previous format's fourcc. Same-thread
  // as the latch in on_stream_process (both on the pw loop).
  self->m_drm_prime_mode = false;
  self->m_sw_format = AV_PIX_FMT_NONE;

  self->notifyFormatChange(new_w, new_h, new_pf);

  const formats::Tag tag = formats::tagFromAvPixFmt(new_pf);
  const int bpp = int(formats::bytesPerPixel(tag));
  const int stride = new_w * bpp;
  // Full frame including chroma planes: stride * h alone under-counts
  // planar formats (NV12 needs 1.5×, P010 3×, P210 4× of w*h), and a
  // producer honouring our suggestion would then deliver buffers the
  // chroma reads in on_stream_process run past.
  const int size = int(formats::frameBytes(tag, new_w, new_h));

  uint8_t buffer[1024];
  spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
  const spa_pod* params[2];

  // Buffers param: the helper picks DmaBuf-vs-SHM dataType for us.
  params[0] = self->m_neg.build_buffers_param(
      b, pc.kind, pc.n_planes, size, stride);
  params[1] = (const spa_pod*)spa_pod_builder_add_object(
      &b, SPA_TYPE_OBJECT_ParamMeta, SPA_PARAM_Meta,
      SPA_PARAM_META_type, SPA_POD_Id(SPA_META_Header),
      SPA_PARAM_META_size, SPA_POD_Int(sizeof(struct spa_meta_header)));

  libremidi::pipewire::load().stream_update_params(
      self->m_data->stream, params, 2);
}

void InputStream::on_stream_process(void* data)
{
  auto* self = static_cast<InputStream*>(data);
  auto& pw = libremidi::pipewire::load();
  if(!pw.stream_available)
    return;

  struct pw_buffer* b = pw.stream_dequeue_buffer(self->m_data->stream);
  if(b == nullptr)
  {
    qWarning() << "PipeWire input: out of buffers";
    return;
  }

  struct spa_buffer* buf = b->buffer;
  if(buf->n_datas == 0)
  {
    pw.stream_queue_buffer(self->m_data->stream, b);
    return;
  }

  // -------- Zero-copy DMA-BUF path (packed RGB + planar YUV) ------
  // Producer delivered a GPU-shareable DMA-BUF. We latch DRM_PRIME
  // mode on the first DmaBuf frame: subsequent frames flow through
  // wrap_dmabuf_as_drm_prime → DRMPrimeDecoder. The first frame is
  // "lost" during the renderer rebuild — acceptable for the
  // zero-copy benefit on every following frame.
  if(buf->datas[0].type == SPA_DATA_DmaBuf)
  {
    if(!self->m_drm_prime_mode)
    {
      // First DmaBuf frame in this stream — latch DRM_PRIME mode.
      // hwaccel_sw_format carries the underlying SW format so the
      // DRMPrimeDecoder picks the right shader (packed RGB vs
      // NV12-shape vs I420).
      self->m_sw_format = self->m_pixelFormat;
      self->hwaccel_sw_format = self->m_pixelFormat;
      self->m_drm_prime_mode = true;
      self->notifyFormatChange(
          self->m_width, self->m_height, AV_PIX_FMT_DRM_PRIME);
      // Drop the first frame so the renderer rebuilds with the new
      // decoder before the next call. queue_buffer back to pipewire.
      pw.stream_queue_buffer(self->m_data->stream, b);
      return;
    }

    const formats::Tag tag = formats::tagFromAvPixFmt(self->m_sw_format);
    AVFrame* f = wrap_dmabuf_as_drm_prime(
        self->m_data->shared, self->m_data->token, b,
        self->m_width, self->m_height,
        self->m_data->format.modifier, tag);
    if(f)
    {
      if(auto* hdr = static_cast<spa_meta_header*>(
             spa_buffer_find_meta_data(
                 buf, SPA_META_Header, sizeof(spa_meta_header))))
      {
        f->pts = int64_t(hdr->pts);
        f->pkt_dts = f->pts;
      }
      {
        std::lock_guard<std::mutex> lock(self->m_frameMutex);
        self->m_usedFrames.push_back(f);
      }
      // pipewire buffer queue-back is deferred to score_pw_release_avframe
      return;
    }
    // Fall through to memcpy path on wrap failure (unsupported format).
  }

  // -------- Sysmem path (MemPtr / MemFd, or planar DmaBuf via mmap)
  // PW_STREAM_FLAG_MAP_BUFFERS gives us a usable `datas[0].data`
  // pointer regardless of underlying SPA type — including DMA-BUF
  // when mmap'd.
  if(buf->datas[0].data == nullptr || buf->datas[0].chunk == nullptr)
  {
    pw.stream_queue_buffer(self->m_data->stream, b);
    return;
  }

  // Drop frames whose mapped payload can't hold what the plane copies
  // below will read. Those copies advance at chunk->stride (not the tight
  // stride), so bound against the ACTUAL stride: an over-large stride
  // from a hostile/misconfigured producer would otherwise pass a
  // tight-size check yet still read past the mmap. plane-0 stride ×
  // (1 for packed, 1.5 for 4:2:0, 2 for 4:2:2) covers Y + chroma.
  {
    const auto* chunk = buf->datas[0].chunk;
    const int64_t tightStride
        = int64_t(self->m_width)
          * formats::bytesPerPixel(formats::tagFromAvPixFmt(self->m_pixelFormat));
    int64_t stride0 = chunk->stride > 0 ? int64_t(chunk->stride) : tightStride;
    if(stride0 < tightStride) // producer under-reports; copies use tightStride
      stride0 = tightStride;

    double planeFactor = 1.0;
    switch(self->m_pixelFormat)
    {
      case AV_PIX_FMT_YUV420P:
      case AV_PIX_FMT_NV12:
      case AV_PIX_FMT_P010LE:
        planeFactor = 1.5;
        break;
      case AV_PIX_FMT_P210LE:
        planeFactor = 2.0;
        break;
      default:
        planeFactor = 1.0; // packed
        break;
    }
    const int64_t neededBytes
        = int64_t(double(stride0) * self->m_height * planeFactor);
    if(neededBytes > 0 && int64_t(chunk->size) < neededBytes)
    {
      pw.stream_queue_buffer(self->m_data->stream, b);
      return;
    }
  }

  AVFrame* frame = nullptr;
  {
    std::lock_guard<std::mutex> lock(self->m_frameMutex);
    if(!self->m_availableFrames.empty())
    {
      frame = self->m_availableFrames.back();
      self->m_availableFrames.pop_back();
    }
  }

  if(!frame)
  {
    pw.stream_queue_buffer(self->m_data->stream, b);
    return;
  }

  // A frame checked out by the renderer across a format renegotiation
  // returns to the pool with the old geometry; copying the new size
  // through its old linesize/allocation would overflow the heap.
  // Reallocate it to the current negotiated state before use.
  if(frame->width != self->m_width || frame->height != self->m_height
     || frame->format != self->m_pixelFormat)
  {
    av_frame_unref(frame);
    frame->format = self->m_pixelFormat;
    frame->width = self->m_width;
    frame->height = self->m_height;
    if(av_frame_get_buffer(frame, 32) < 0)
    {
      av_frame_free(&frame);
      pw.stream_queue_buffer(self->m_data->stream, b);
      return;
    }
  }

  // Copy from pipewire-mapped sysmem into the AVFrame. We honour the
  // chunk's stride when present (server may have row-padded); fall back
  // to size/height when stride is zero (common for tightly-packed
  // server buffers).
  auto* src = static_cast<const uint8_t*>(buf->datas[0].data);
  const auto* chunk = buf->datas[0].chunk;
  // Leave the raw stride (possibly <= 0) for each case to default from:
  // the packed and planar branches below set the correct per-format
  // stride when the producer reports none. A blanket w*4 default here
  // would shadow those and corrupt the planar copies.
  int src_stride = chunk->stride;

  const int w = self->m_width;
  const int h = self->m_height;
  const AVPixelFormat fmt = self->m_pixelFormat;

  switch(fmt)
  {
    // Packed RGB / YUV (8-bit), packed 10/16-bit RGB: single plane,
    // single contiguous buffer. Use the PADDED bits-per-pixel: the
    // plain av_get_bits_per_pixel sums component depths only, so the
    // X2RGB10/X2BGR10 formats come out as 30 bits → bpp 3 and the copy
    // silently drops the right quarter of every row.
    case AV_PIX_FMT_RGB24:
    case AV_PIX_FMT_RGBA:
    case AV_PIX_FMT_BGRA:
    case AV_PIX_FMT_YUYV422:
    case AV_PIX_FMT_UYVY422:
    case AV_PIX_FMT_X2RGB10LE:
    case AV_PIX_FMT_X2BGR10LE:
    case AV_PIX_FMT_RGBA64LE:
    {
      const int bpp
          = (av_get_padded_bits_per_pixel(av_pix_fmt_desc_get(fmt)) + 7) / 8;
      if(src_stride <= 0)
        src_stride = w * bpp;

      // Half-float wire (SPA RGBA_F16): there is no packed half-float
      // AVFrame format with wide support, so the frame is RGBA64LE
      // (16-bit unsigned). Convert sample-by-sample instead of memcpy —
      // a raw copy would hand IEEE-754 half bit patterns to consumers
      // expecting integers.
      if(fmt == AV_PIX_FMT_RGBA64LE
         && self->m_data->format.format == SPA_VIDEO_FORMAT_RGBA_F16)
      {
        auto halfToU16 = [](uint16_t hbits) -> uint16_t {
          const uint32_t sign = (hbits >> 15) & 1;
          const uint32_t exp = (hbits >> 10) & 0x1F;
          const uint32_t mant = hbits & 0x3FF;
          float v;
          if(exp == 0)
            v = std::ldexp(float(mant), -24); // subnormal
          else if(exp == 31)
            v = mant ? 0.f : 1.f; // NaN → 0, inf → clamp
          else
            v = std::ldexp(float(mant + 1024), int(exp) - 25);
          if(sign)
            v = 0.f; // negative → clamp to 0
          v = std::min(v, 1.f);
          return uint16_t(std::lround(v * 65535.f));
        };
        for(int y = 0; y < h; ++y)
        {
          const auto* srow = reinterpret_cast<const uint16_t*>(
              src + std::size_t(y) * src_stride);
          auto* drow = reinterpret_cast<uint16_t*>(
              frame->data[0] + std::size_t(y) * frame->linesize[0]);
          for(int x = 0; x < w * 4; ++x)
            drow[x] = halfToU16(srow[x]);
        }
        break;
      }

      av_image_copy_plane(
          frame->data[0], frame->linesize[0], src, src_stride, w * bpp, h);
      break;
    }

    // I420 / YUV420P: 3 planes. PipeWire packs them sequentially in
    // datas[0].data when transmitting via shared sysmem. Layout:
    //   Y: w × h
    //   U: (w/2) × (h/2)
    //   V: (w/2) × (h/2)
    case AV_PIX_FMT_YUV420P:
    {
      if(src_stride <= 0)
        src_stride = w;
      const std::size_t ySize = std::size_t(src_stride) * h;
      const int chromaStride = src_stride / 2;
      const std::size_t uvSize = std::size_t(chromaStride) * (h / 2);
      // YV12 carries V before U on the wire; the AVFrame is always
      // I420-ordered (data[1]=U, data[2]=V), so swap the source planes.
      const bool yv12 = self->m_data->format.format == SPA_VIDEO_FORMAT_YV12;
      const uint8_t* srcU = src + ySize + (yv12 ? uvSize : 0);
      const uint8_t* srcV = src + ySize + (yv12 ? 0 : uvSize);

      av_image_copy_plane(
          frame->data[0], frame->linesize[0], src, src_stride, w, h);
      av_image_copy_plane(
          frame->data[1], frame->linesize[1], srcU, chromaStride, w / 2, h / 2);
      av_image_copy_plane(
          frame->data[2], frame->linesize[2], srcV, chromaStride, w / 2, h / 2);
      break;
    }

    // NV12: 2 planes. Y plane then UV interleaved at half height.
    case AV_PIX_FMT_NV12:
    {
      if(src_stride <= 0)
        src_stride = w;
      const std::size_t ySize = std::size_t(src_stride) * h;
      const uint8_t* srcUV = src + ySize;
      av_image_copy_plane(
          frame->data[0], frame->linesize[0], src, src_stride, w, h);
      av_image_copy_plane(
          frame->data[1], frame->linesize[1], srcUV, src_stride, w, h / 2);
      break;
    }

    // P010 / P012: 2 planes, 16-bit lanes (10/12-bit data in MSBs).
    // Layout matches NV12 but every sample is 2 bytes:
    //   Y:  stride × h
    //   UV: stride × (h/2)  (UV pairs interleaved as u16 each)
    case AV_PIX_FMT_P010LE:
    {
      if(src_stride <= 0)
        src_stride = w * 2;
      const std::size_t ySize = std::size_t(src_stride) * h;
      const uint8_t* srcUV = src + ySize;
      av_image_copy_plane(
          frame->data[0], frame->linesize[0], src, src_stride, w * 2, h);
      av_image_copy_plane(
          frame->data[1], frame->linesize[1], srcUV, src_stride, w * 2, h / 2);
      break;
    }

    // P210: P010-shape with full-height chroma (4:2:2 subsampling).
    case AV_PIX_FMT_P210LE:
    {
      if(src_stride <= 0)
        src_stride = w * 2;
      const std::size_t ySize = std::size_t(src_stride) * h;
      const uint8_t* srcUV = src + ySize;
      av_image_copy_plane(
          frame->data[0], frame->linesize[0], src, src_stride, w * 2, h);
      av_image_copy_plane(
          frame->data[1], frame->linesize[1], srcUV, src_stride, w * 2, h);
      break;
    }

    default:
      // Unsupported format: return frame unused.
      std::lock_guard<std::mutex> lock(self->m_frameMutex);
      self->m_availableFrames.push_back(frame);
      pw.stream_queue_buffer(self->m_data->stream, b);
      return;
  }

  // Timestamp from the SPA header meta (same as the DRM-PRIME path).
  // chunk->offset is a byte offset into the buffer, not a time.
  if(auto* hdr = static_cast<spa_meta_header*>(spa_buffer_find_meta_data(
         buf, SPA_META_Header, sizeof(spa_meta_header))))
  {
    frame->pts = int64_t(hdr->pts);
  }
  else
  {
    frame->pts = 0;
  }
  frame->pkt_dts = frame->pts;

  {
    std::lock_guard<std::mutex> lock(self->m_frameMutex);
    self->m_usedFrames.push_back(frame);
  }

  pw.stream_queue_buffer(self->m_data->stream, b);
}

// ============================================================================
// PipeWireDevice — wraps an InputStream in score's device protocol
// ============================================================================

class PipeWireDevice final : public GfxInputDevice
{
  W_OBJECT(PipeWireDevice)
public:
  using GfxInputDevice::GfxInputDevice;
  ~PipeWireDevice();

  bool reconnect() override;
  ossia::net::device_base* getDevice() const override { return m_dev.get(); }

private:
  Gfx::video_texture_input_protocol* m_protocol{};
  mutable std::unique_ptr<Gfx::video_texture_input_device> m_dev;
};

class PipeWireSettingsWidget final : public Device::ProtocolSettingsWidget
{
public:
  PipeWireSettingsWidget(QWidget* parent = nullptr);
  Device::DeviceSettings getSettings() const override;
  void setSettings(const Device::DeviceSettings& settings) override;

private:
  void updatePath();

  QLineEdit* m_nodeEdit{};
  QSpinBox* m_widthEdit{};
  QSpinBox* m_heightEdit{};
  QDoubleSpinBox* m_fpsEdit{};
  QComboBox* m_formatEdit{};
};

PipeWireDevice::~PipeWireDevice() = default;

bool PipeWireDevice::reconnect()
{
  disconnect();
  try
  {
    auto set = m_settings.deviceSpecificSettings.value<SharedInputSettings>();
    auto* plug = m_ctx.findPlugin<Gfx::DocumentPlugin>();
    if(plug)
    {
      auto stream = std::make_shared<InputStream>(set.path);
      m_protocol
          = new Gfx::video_texture_input_protocol{std::move(stream), plug->exec};
      m_dev = std::make_unique<Gfx::video_texture_input_device>(
          std::unique_ptr<ossia::net::protocol_base>(m_protocol),
          this->settings().name.toStdString());
    }
  }
  catch(std::exception& e)
  {
    qDebug() << "PipeWire reconnect failed:" << e.what();
  }
  catch(...)
  {
    qDebug() << "PipeWire reconnect: unknown error";
  }

  // Notify the device tree that connection state has changed so the UI
  // refreshes its connection indicator. This was the "FIXME changed();"
  // marker in the old port.
  deviceChanged(nullptr, m_dev.get());
  return connected();
}

PipeWireSettingsWidget::PipeWireSettingsWidget(QWidget* parent)
    : Device::ProtocolSettingsWidget(parent)
{
  m_nodeEdit = new QLineEdit(this);
  m_widthEdit = new QSpinBox(this);
  m_heightEdit = new QSpinBox(this);
  m_fpsEdit = new QDoubleSpinBox(this);
  m_formatEdit = new QComboBox(this);

  m_nodeEdit->setPlaceholderText("Leave empty for auto-connect");

  m_widthEdit->setRange(1, 7680);
  m_widthEdit->setValue(1920);
  m_heightEdit->setRange(1, 4320);
  m_heightEdit->setValue(1080);
  m_fpsEdit->setRange(1.0, 240.0);
  m_fpsEdit->setValue(30.0);
  m_fpsEdit->setSuffix(" fps");
  m_formatEdit->addItems(
      {"rgb24", "rgba", "bgra", "rgb10a2", "bgr10a2", "rgba16f",
       "p010", "p210", "yuv420p", "yuyv422", "uyvy422", "nv12"});

  auto* layout = new QFormLayout;
  layout->addRow(tr("PipeWire Node:"), m_nodeEdit);
  layout->addRow(tr("Width:"), m_widthEdit);
  layout->addRow(tr("Height:"), m_heightEdit);
  layout->addRow(tr("Frame Rate:"), m_fpsEdit);
  layout->addRow(tr("Pixel Format:"), m_formatEdit);
  setLayout(layout);

  setSettings(InputFactory{}.defaultSettings());
}

void PipeWireSettingsWidget::updatePath() { }

Device::DeviceSettings PipeWireSettingsWidget::getSettings() const
{
  Device::DeviceSettings s;
  s.protocol = InputFactory::static_concreteKey();
  s.name = "PipeWire";

  SharedInputSettings set;
  QString path = "pipewire://" + m_nodeEdit->text();
  const QStringList params{
      QString("width=%1").arg(m_widthEdit->value()),
      QString("height=%1").arg(m_heightEdit->value()),
      QString("fps=%1").arg(m_fpsEdit->value()),
      QString("format=%1").arg(m_formatEdit->currentText())};
  path += "?" + params.join("&");
  set.path = path;
  s.deviceSpecificSettings = QVariant::fromValue(set);
  return s;
}

void PipeWireSettingsWidget::setSettings(const Device::DeviceSettings& settings)
{
  if(!settings.deviceSpecificSettings.canConvert<SharedInputSettings>())
    return;
  const auto set = settings.deviceSpecificSettings.value<SharedInputSettings>();

  static const QRegularExpression re("pipewire://([^?]*)(?:\\?(.*))?");
  auto m = re.match(set.path);
  if(!m.hasMatch())
    return;

  m_nodeEdit->setText(m.captured(1));
  const QString params = m.captured(2);
  if(params.isEmpty())
    return;

  static const QRegularExpression paramRe("([^=&]+)=([^&]+)");
  auto it = paramRe.globalMatch(params);
  while(it.hasNext())
  {
    const auto pm = it.next();
    const QString k = pm.captured(1), v = pm.captured(2);
    if(k == "width")
      m_widthEdit->setValue(v.toInt());
    else if(k == "height")
      m_heightEdit->setValue(v.toInt());
    else if(k == "fps")
      m_fpsEdit->setValue(v.toDouble());
    else if(k == "format")
    {
      const int idx = m_formatEdit->findText(v);
      if(idx >= 0)
        m_formatEdit->setCurrentIndex(idx);
    }
  }
}

// ============================================================================
// Device enumerators
// ============================================================================

namespace
{

/** Enumerates Video/Source nodes published on the local pipewire daemon.
 *
 *  Backed by the process-wide shared `libremidi::pipewire::context`.
 *  We snapshot the current registry view (already populated by the
 *  shared context's thread_loop) and filter by media_class::video.
 *  The shared context's snapshot is internally locked, so this is
 *  safe to call from the UI thread. */
class PipeWireSourceEnumerator : public Device::DeviceEnumerator
{
public:
  void enumerate(
      std::function<void(const QString&, const Device::DeviceSettings&)> f)
      const override
  {
    auto shared = libremidi::pipewire::shared_context();
    if(!shared || !shared->ok())
      return;

    // snapshot() locks internally and copies; we filter on the
    // returned value without holding the loop lock.
    const auto snap = shared->snapshot();
    for(const auto& node : snap.nodes_of(libremidi::pipewire::media_class::video))
    {
      // Filter for Sources only (skip Sinks like screen-share consumers).
      // media_class_str is the raw "Video/Source" / "Video/Sink"
      // string captured at registration time.
      if(node.media_class_str.find("Source") == std::string::npos)
        continue;

      const QString nodeName = QString::fromStdString(node.name);
      const QString nodeDescr = QString::fromStdString(node.description);
      const QString display
          = !nodeDescr.isEmpty() ? nodeDescr
            : !nodeName.isEmpty() ? nodeName
                                  : QStringLiteral("(unnamed)");

      Device::DeviceSettings s;
      s.protocol = InputFactory::static_concreteKey();
      s.name = display;
      SharedInputSettings sis;
      sis.path = "pipewire://" + nodeName
                 + "?width=1920&height=1080&fps=30&format=rgba";
      s.deviceSpecificSettings = QVariant::fromValue(sis);
      f(s.name, s);
    }
  }
};

/** Single-entry "Default" enumerator — emits one settings struct with
 *  an empty target so pipewire auto-selects the default Video/Source.
 *  Mirrors `DefaultCameraEnumerator` in CameraDevice.cpp. */
class DefaultPipeWireSourceEnumerator : public Device::DeviceEnumerator
{
public:
  void enumerate(
      std::function<void(const QString&, const Device::DeviceSettings&)> f)
      const override
  {
    Device::DeviceSettings s;
    s.protocol = InputFactory::static_concreteKey();
    s.name = "Default PipeWire Source";
    SharedInputSettings sis;
    sis.path = "pipewire://?width=1920&height=1080&fps=30&format=rgba";
    s.deviceSpecificSettings = QVariant::fromValue(sis);
    f(s.name, s);
  }
};

} // namespace

// ============================================================================
// InputFactory
// ============================================================================

QString InputFactory::prettyName() const noexcept
{
  return QObject::tr("PipeWire Video Input");
}

QUrl InputFactory::manual() const noexcept
{
  return QUrl{
      "https://ossia.io/score-docs/devices/input-devices.html#pipewire-input"};
}

Device::DeviceInterface* InputFactory::makeDevice(
    const Device::DeviceSettings& settings,
    const Explorer::DeviceDocumentPlugin& /*plugin*/,
    const score::DocumentContext& ctx)
{
  return new PipeWireDevice(settings, ctx);
}

const Device::DeviceSettings& InputFactory::defaultSettings() const noexcept
{
  static const Device::DeviceSettings s = [] {
    Device::DeviceSettings d;
    d.name = "PipeWire";
    d.protocol = InputFactory::static_concreteKey();
    SharedInputSettings set;
    set.path = "pipewire://?width=1920&height=1080&fps=30&format=rgba";
    d.deviceSpecificSettings = QVariant::fromValue(set);
    return d;
  }();
  return s;
}

Device::ProtocolSettingsWidget* InputFactory::makeSettingsWidget()
{
  return new PipeWireSettingsWidget;
}

std::shared_ptr<::Video::ExternalInput> makePipewireCapture(const QString& path)
{
  return std::make_shared<InputStream>(path);
}

QString pipewireInputNegotiatedTransport(::Video::ExternalInput& input)
{
  if(auto* s = dynamic_cast<InputStream*>(&input))
  {
    switch(s->m_negotiatedKind.load(std::memory_order_relaxed))
    {
      case 1:
        return QStringLiteral("shm");
      case 2:
        return QStringLiteral("dmabuf");
      default:
        return QStringLiteral("none");
    }
  }
  return {};
}

Device::DeviceEnumerators
InputFactory::getEnumerators(const score::DocumentContext& /*ctx*/) const
{
  Device::DeviceEnumerators enums;
  enums.push_back({"Default", new DefaultPipeWireSourceEnumerator});
  enums.push_back({"Sources", new PipeWireSourceEnumerator});
  return enums;
}

} // namespace Gfx::PipeWire

W_OBJECT_IMPL(Gfx::PipeWire::PipeWireDevice)
