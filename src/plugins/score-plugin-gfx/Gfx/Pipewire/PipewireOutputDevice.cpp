// SPDX-License-Identifier: GPL-3.0-or-later
//
// PipeWire video output device: renders score's graph offscreen into a QRhi
// texture at the configured pixel format and publishes each frame as a PipeWire
// stream buffer (PW_DIRECTION_OUTPUT, media-class Video/Source). The score side
// mirrors ShmdataOutputDevice: a GfxOutputDevice wrapping an ossia device whose
// root node is a PipewireOutputNode.
//
// Producer shape follows pipewire's src/examples/video-src.c: pw_thread_loop
// rather than pw_main_loop in a std::thread, and the format announced at
// connect time as a single EnumFormat mapped from formats::Tag.
//
// Three publication modes, selected from rhi->backend() and the URL's ?dmabuf=:
//
//   Vulkan DMA-BUF: exportable VkImages created with
//   VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, their FDs handed to
//   pipewire via PW_STREAM_FLAG_ALLOC_BUFFERS + on_add_buffer. Per render:
//   render into the QRhi texture, one GPU copyTexture into the pipewire
//   buffer's image, then queue_buffer.
//
//   EGL/GBM DMA-BUF: symmetric, with GBM BOs on /dev/dri/renderD128 imported
//   back into the local EGL display so the renderer can target them through
//   QRhi GL createFrom.
//
//   CPU readback (default, every backend): render, read back, memcpy into the
//   server-allocated SHM buffers mapped by PW_STREAM_FLAG_MAP_BUFFERS.
//
// Both DMA-BUF modes advertise SPA_FORMAT_VIDEO_modifier =
// DRM_FORMAT_MOD_LINEAR. Rendering directly into the external image would need
// a QRhiTextureRenderTarget rebuild per pipewire-buffer dequeue, which fights
// QRhi's render-target caching; the copyTexture bridge still drops the CPU
// readback and memcpy.

#include <ossia/detail/hash_map.hpp>

#include <atomic>
#include <chrono>
#include <mutex>
#include <vector>
#include "PipewireOutputDevice.hpp"

#include "PipewireFormats.hpp"
#include "PipewireSharedContext.hpp"

#include <libremidi/backends/linux/pipewire/context.hpp>
#include <libremidi/backends/linux/pipewire/loader.hpp>

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/GfxExecContext.hpp>
#include <Gfx/GfxParameter.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/OutputNode.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/RenderState.hpp>
#include <Gfx/InvertYRenderer.hpp>
#include <Gfx/SharedOutputSettings.hpp>

#if defined(__linux__)
#include <Gfx/Graph/interop/EglDmaBufExport.hpp>
#include <Gfx/Graph/interop/EglDmaBufImport.hpp>
#include <Gfx/Graph/interop/VkExternalMemoryHelpers.hpp>
#if QT_HAS_VULKAN && QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
#include <QtGui/private/qrhivulkan_p.h>

#include <QVulkanFunctions>
#define SCORE_PIPEWIRE_OUT_DMABUF 1
#endif
// EGL DMA-BUF output is available whenever EglDmaBufImport works.
// Independent of Vulkan availability — the OutputNode picks one at
// runtime based on `rhi->backend()`.
#define SCORE_PIPEWIRE_OUT_DMABUF_EGL 1
#endif

#include <QRegularExpression>

#include <score/document/DocumentContext.hpp>
#include <score/gfx/OpenGL.hpp>
#include <score/gfx/QRhiGles2.hpp>

#include <ossia/network/base/device.hpp>
#include <ossia/network/base/protocol.hpp>

#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QOffscreenSurface>
#include <QObject>
#include <QSpinBox>

#include <pipewire/pipewire.h>
#include <spa/param/buffers.h>
#include <spa/param/video/format-utils.h>

#include <wobjectimpl.h>

#include <cstring>
#include <memory>
#include <unordered_map>

#if defined(__linux__)
#include <unistd.h> // close()
#endif

namespace Gfx::PipeWire
{
//! DRM_FORMAT_MOD_INVALID: "whatever layout the driver chose". A consumer that
//! does not negotiate explicit modifiers imports this one.
static constexpr int64_t kDrmModifierInvalid = int64_t((1ull << 56) - 1);


// ============================================================================
// PipewireProducer — wraps pw_thread_loop + pw_stream for output
// ============================================================================

namespace
{

/** Thin RAII wrapper around a pipewire output stream. Owns its own
 *  thread loop so multiple producers in the same process don't
 *  contend for a shared main loop. Frames are pushed via push_frame()
 *  from the score render thread; the wrapper grabs the thread loop
 *  lock before calling into pipewire. */
/** When DMA-BUF mode is enabled, the producer allocates its own
 *  exportable images and hands them to pipewire as DMA-BUF FDs.
 *  Each pipewire buffer is associated with one of these images for
 *  the lifetime of the stream; on_add_buffer / on_remove_buffer
 *  manage allocation. */
struct DmaBufSlot
{
#if defined(SCORE_PIPEWIRE_OUT_DMABUF)
  score::gfx::vkinterop::ExternalImage img{};
  int fd{-1};
  uint32_t rowPitch{0}; ///< LINEAR image row pitch from the driver.
#endif
};

#if defined(SCORE_PIPEWIRE_OUT_DMABUF_EGL)
/** EGL/GBM variant. The GBM bo + FD lifecycle lives on the pipewire
 *  thread (in on_add_buffer / on_remove_buffer); the EGL import + GL
 *  texture creation lives on the render thread (GL contexts are
 *  thread-local). The render thread drains the pending-import /
 *  pending-cleanup queues under the pipewire thread-loop lock at the
 *  top of every render() — see PipewireProducer::process_pending_egl_gl. */
struct DmaBufEglSlot
{
  score::gfx::GbmDmaBufExport::Slot slot{};
  bool needs_import{false};
  bool needs_cleanup{false};
  bool ready{false};
};
#endif

enum class DmaBufBackend : std::uint8_t
{
  None,
  Vulkan,
  EglGbm,
};

class PipewireProducer
{
public:
  PipewireProducer(
      int width, int height, double fps, formats::Tag fmt,
      QString node_name)
      : m_width(width)
      , m_height(height)
      , m_fps(fps)
      , m_fmt(fmt)
      , m_bpp(int(formats::bytesPerPixel(fmt)))
      , m_nodeName(std::move(node_name))
  {
    // pw_init / deinit lifecycle is owned by the shared context's
    // libremidi::pipewire::instance singleton — no per-producer init.
  }

  // Convenience accessor for the underlying thread loop. Lock/unlock
  // pairs in dequeue/queue spans use this directly since the manual
  // lock has to straddle two method calls (RAII guards don't fit).
  pw_thread_loop* loop() const noexcept
  {
    return m_shared ? m_shared->thread_loop_handle() : nullptr;
  }

#if defined(SCORE_PIPEWIRE_OUT_DMABUF)
  /** Enable DMA-BUF mode (Vulkan backend). Must be called BEFORE start().
   *  The supplied VulkanCtx is used to allocate exportable VkImages in
   *  on_add_buffer; caller owns the VulkanCtx and must keep it alive
   *  for the producer's lifetime. */
  void enable_dmabuf_mode(const score::gfx::vkinterop::VulkanCtx& vk) noexcept
  {
    m_vk = vk;
    m_dmabufBackend = DmaBufBackend::Vulkan;
    enumerate_modifiers();
  }

  /** Every DRM format modifier the driver can render our wire format with.
   *
   *  These are the layouts we can offer a consumer, and the reason to offer
   *  them: a modifier-tiled export is written and copied into far faster than
   *  a LINEAR one in host memory (see
   *  tests/hardware/dmabuf-export-bandwidth.cpp). An empty list is not a
   *  failure -- it means the driver has no shareable tiled layout for the
   *  format, and LINEAR is what we fall back to.
   */
  void enumerate_modifiers() noexcept
  {
    m_drmModifiers.clear();
    if(!m_vk.qInst || !m_vk.physDev)
      return;
    auto getProps2 = (PFN_vkGetPhysicalDeviceFormatProperties2)
        m_vk.qInst->getInstanceProcAddr("vkGetPhysicalDeviceFormatProperties2");
    if(!getProps2)
      return;

    VkDrmFormatModifierPropertiesListEXT list{};
    list.sType = VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT;
    VkFormatProperties2 props{};
    props.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2;
    props.pNext = &list;

    const VkFormat fmt = vkFormatFromTag(m_fmt);
    getProps2(m_vk.physDev, fmt, &props);
    if(list.drmFormatModifierCount == 0)
      return;

    std::vector<VkDrmFormatModifierPropertiesEXT> mods(
        list.drmFormatModifierCount);
    list.pDrmFormatModifierProperties = mods.data();
    getProps2(m_vk.physDev, fmt, &props);

    for(const auto& m : mods)
    {
      // It has to be usable as the thing we render into and copy to.
      const auto need = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT
                        | VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
      if((m.drmFormatModifierTilingFeatures & need) == need)
        m_drmModifiers.push_back(m.drmFormatModifier);
    }
  }
#endif

#if defined(SCORE_PIPEWIRE_OUT_DMABUF_EGL)
  /** Enable DMA-BUF mode (EGL/GBM backend). Must be called BEFORE start().
   *  The importer is borrowed (caller owns; render-thread affinity).
   *  drm_fourcc is one of GbmDmaBufExport::DRM_FORMAT_*_v. */
  bool enable_dmabuf_mode_egl(
      score::gfx::EglDmaBufImporter* importer, uint32_t drm_fourcc) noexcept
  {
    if(!importer)
      return false;
    if(!m_gbmExporter.init())
      return false;
    m_eglImporter = importer;
    m_drmFourcc = drm_fourcc;
    m_dmabufBackend = DmaBufBackend::EglGbm;
    return true;
  }

  /** Render-thread tick: drain GL-touching work that on_add_buffer /
   *  on_remove_buffer deferred. MUST be called with the thread-loop
   *  lock held — dmabuf_dequeue_egl does this for you. */
  void process_pending_egl_gl() noexcept
  {
    if(m_dmabufBackend != DmaBufBackend::EglGbm || !m_eglImporter)
      return;
    for(auto it = m_eglSlots.begin(); it != m_eglSlots.end(); )
    {
      auto& s = it->second;
      if(s.needs_cleanup)
      {
        // GBM/FD were already released on pipewire thread; only the
        // GL/EGL resources remain to scrub.
        if(s.slot.plane.image)
          m_eglImporter->cleanupPlane(s.slot.plane);
        if(s.slot.glTexture)
        {
          if(auto* ctx = QOpenGLContext::currentContext(); ctx)
            ctx->extraFunctions()->glDeleteTextures(1, &s.slot.glTexture);
          s.slot.glTexture = 0;
        }
        it = m_eglSlots.erase(it);
        continue;
      }
      if(s.needs_import)
      {
        // Re-import the FD into our local EGL display + bind to a new
        // GL texture id. Slot's gbm fields (fd/stride/offset/modifier)
        // were populated on pipewire thread; we just consume them.
        if(auto* ctx = QOpenGLContext::currentContext(); ctx)
        {
          s.slot.glTexture = score::gfx::createLinearClampGlTexture2D();
          if(s.slot.glTexture
             && m_eglImporter->importPlane(
                 s.slot.plane, s.slot.glTexture, s.slot.fd,
                 s.slot.modifier, s.slot.offset, s.slot.stride,
                 s.slot.drm_fourcc, int(s.slot.width), int(s.slot.height)))
          {
            s.ready = true;
          }
          else
          {
            qWarning() << "PipewireProducer: EGL importPlane failed";
            if(s.slot.glTexture)
              ctx->extraFunctions()->glDeleteTextures(1, &s.slot.glTexture);
            s.slot.glTexture = 0;
          }
        }
        s.needs_import = false;
      }
      ++it;
    }
  }

  /** EGL dequeue: render thread variant. Returns the GL texture id
   *  associated with this buffer. The loop lock is only held inside
   *  this call; it is re-taken by dmabuf_queue_egl. */
  pw_buffer* dmabuf_dequeue_egl(unsigned int* out_glTexture) noexcept
  {
    auto* lp = loop();
    if(!lp || !m_stream || m_dmabufBackend != DmaBufBackend::EglGbm)
      return nullptr;
    auto& pw = libremidi::pipewire::load();
    if(!pw.stream_available || !m_streaming.load(std::memory_order_acquire))
      return nullptr;
    pw.thread_loop_lock(lp);

    // Run any deferred GL imports / cleanups while we have the loop
    // lock (synchronises against on_add_buffer / on_remove_buffer).
    process_pending_egl_gl();

    pw_buffer* b = pw.stream_dequeue_buffer(m_stream);
    if(!b)
    {
      pw.thread_loop_unlock(lp);
      return nullptr;
    }
    auto it = m_eglSlots.find(b);
    if(it == m_eglSlots.end() || !it->second.ready)
    {
      // Slot's GL import hasn't happened yet (deferred), or no slot.
      // Drop this frame; pipewire pacing absorbs the loss.
      pw.stream_queue_buffer(m_stream, b);
      pw.thread_loop_unlock(lp);
      return nullptr;
    }
    *out_glTexture = it->second.slot.glTexture;
    // Release the loop lock for the render + glFinish window (see the
    // Vulkan path). Safe against on_remove_buffer_egl: it keeps the map
    // entry (needs_cleanup) and the imported EGLImage pins the dmabuf
    // pages, so the GL texture stays writable until process_pending.
    pw.thread_loop_unlock(lp);
    return b;
  }

  void dmabuf_queue_egl(pw_buffer* b) noexcept
  {
    if(!b)
      return;
    auto& pw = libremidi::pipewire::load();
    auto* lp = loop();
    if(!lp)
      return;
    pw.thread_loop_lock(lp);
    if(pw.stream_available && m_stream)
    {
      auto it = m_eglSlots.find(b);
      // needs_cleanup means the server removed the buffer while we were
      // rendering: b is dangling, drop the frame instead of queueing.
      if(it != m_eglSlots.end() && !it->second.needs_cleanup)
      {
        auto* chunk = b->buffer->datas[0].chunk;
        chunk->offset = uint32_t(it->second.slot.offset);
        chunk->size = uint32_t(it->second.slot.size);
        chunk->stride = int32_t(it->second.slot.stride);
        pw.stream_queue_buffer(m_stream, b);
        m_framesQueued.fetch_add(1, std::memory_order_relaxed);
      }
    }
    pw.thread_loop_unlock(lp);
  }
#endif

#if defined(SCORE_PIPEWIRE_OUT_DMABUF)

  /** Dequeue a pipewire buffer for filling. Returns the buf pointer
   *  and writes the associated VkImage handle to `out_image`. The loop
   *  lock is only held inside this call — the render → copy → GPU-wait
   *  window runs unlocked so the pipewire data thread keeps servicing
   *  its loop; dmabuf_queue re-takes the lock. */
  pw_buffer* dmabuf_dequeue(VkImage* out_image, VkDeviceMemory* out_memory) noexcept
  {
    auto* lp = loop();
    if(!lp || !m_stream || m_dmabufBackend != DmaBufBackend::Vulkan)
      return nullptr;
    auto& pw = libremidi::pipewire::load();
    if(!pw.stream_available || !m_streaming.load(std::memory_order_acquire))
      return nullptr;
    pw.thread_loop_lock(lp);
    pw_buffer* b = pw.stream_dequeue_buffer(m_stream);
    if(!b)
    {
      pw.thread_loop_unlock(lp);
      return nullptr;
    }
    // Look up the slot we allocated for this buffer in on_add_buffer.
    auto it = m_dmabufSlots.find(b);
    if(it == m_dmabufSlots.end())
    {
      pw.stream_queue_buffer(m_stream, b);
      pw.thread_loop_unlock(lp);
      return nullptr;
    }
    *out_image = it->second.img.image;
    *out_memory = it->second.img.memory;
    // Release the loop lock for the render + GPU-sync window; holding it
    // across endOffscreenFrame stalls the pipewire data thread for the
    // whole GPU wait (xruns). on_remove_buffer defers this buffer's
    // teardown to dmabuf_queue while it is checked out.
    m_inFlight = b;
    m_inFlightRemoved = false;
    pw.thread_loop_unlock(lp);
    return b;
  }

  /** Commit a previously-dequeued buffer back to pipewire (making it
   *  visible to the consumer). Takes the thread-loop lock. */
  void dmabuf_queue(pw_buffer* b) noexcept
  {
    if(!b)
      return;
    auto& pw = libremidi::pipewire::load();
    auto* lp = loop();
    if(!lp)
      return;
    pw.thread_loop_lock(lp);
    if(m_inFlightRemoved)
    {
      // The server removed this buffer while we were rendering into it;
      // b is dangling (pipewire frees it after on_remove_buffer), so
      // only destroy the deferred image and drop the frame.
      if(m_inFlightDeferred.fd >= 0)
        ::close(m_inFlightDeferred.fd);
      score::gfx::vkinterop::destroyExternal(m_vk, m_inFlightDeferred.img);
      m_inFlightDeferred = {};
      m_inFlightRemoved = false;
      m_inFlight = nullptr;
      pw.thread_loop_unlock(lp);
      return;
    }
    m_inFlight = nullptr;
    if(pw.stream_available && m_stream)
    {
      auto it = m_dmabufSlots.find(b);
      if(it != m_dmabufSlots.end())
      {
        auto* chunk = b->buffer->datas[0].chunk;
        chunk->offset = 0;
        chunk->size = uint32_t(it->second.img.size);
        // Use the driver-reported LINEAR row pitch; width*bpp shears on
        // pitch-padded allocations.
        chunk->stride = it->second.rowPitch > 0
                            ? int32_t(it->second.rowPitch)
                            : int32_t(m_width * m_bpp);
      }
      pw.stream_queue_buffer(m_stream, b);
      m_framesQueued.fetch_add(1, std::memory_order_relaxed);
    }
    pw.thread_loop_unlock(lp);
  }
#endif

  ~PipewireProducer()
  {
    stop();
#if defined(SCORE_PIPEWIRE_OUT_DMABUF_EGL)
    // After stop() the pipewire thread is gone; on_remove_buffer_egl
    // marked every slot as needs_cleanup. Scrub the GL/EGL resources
    // here on the render thread (caller). The pipewire-owned fields
    // (fd, bo) were released synchronously by on_remove_buffer_egl.
    if(m_dmabufBackend == DmaBufBackend::EglGbm && m_eglImporter)
    {
      for(auto& [pw, s] : m_eglSlots)
      {
        if(s.slot.plane.image)
          m_eglImporter->cleanupPlane(s.slot.plane);
        if(s.slot.glTexture)
        {
          if(auto* ctx = QOpenGLContext::currentContext(); ctx)
            ctx->extraFunctions()->glDeleteTextures(1, &s.slot.glTexture);
          s.slot.glTexture = 0;
        }
      }
      m_eglSlots.clear();
      m_gbmExporter.shutdown();
    }
#endif
    // pw_deinit lifecycle is owned by the shared context's instance
    // singleton — releasing m_shared decrements its refcount.
    m_shared.reset();
  }

  bool start()
  {
    auto& pw = libremidi::pipewire::load();
    if(!pw.stream_available)
    {
      qWarning()
          << "PipewireProducer: libpipewire-0.3 stream API not available";
      return false;
    }

    // Acquire the process-wide shared connection (also used by
    // libossia audio, libremidi MIDI, score's PipeWire input device).
    m_shared.reset();
    m_shared = acquireSharedContext("PipewireProducer");
    if(!m_shared)
    {
      qWarning() << "PipewireProducer: failed to acquire shared context";
      return false;
    }

    auto* lp = m_shared->thread_loop_handle();
    pw.thread_loop_lock(lp);

    // media.class is what decides whether anything outside score can use this.
    // Left to itself an output stream is classed "Stream/Output/Video", and a
    // session manager only links a video consumer -- OBS, GStreamer's
    // pipewiresrc, another application -- to a node classed "Video/Source":
    // the format negotiates against the node's parameters and then not one
    // buffer ever moves, which is exactly what those consumers saw.
    auto* props = pw.properties_new(
        PW_KEY_MEDIA_TYPE, "Video", PW_KEY_MEDIA_CATEGORY, "Source",
        PW_KEY_MEDIA_ROLE, "Camera", PW_KEY_MEDIA_CLASS, "Video/Source", nullptr);
    if(!m_nodeName.isEmpty())
    {
      pw.properties_set(
          props, PW_KEY_NODE_NAME, m_nodeName.toUtf8().constData());
      // What a consumer's device list shows. Without it the entry is blank.
      pw.properties_set(
          props, PW_KEY_NODE_DESCRIPTION, m_nodeName.toUtf8().constData());
    }

    m_stream = pw.stream_new(m_shared->pw_core_ptr(), "score-output", props);
    if(!m_stream)
    {
      pw.thread_loop_unlock(lp);
      stop();
      return false;
    }

    static const pw_stream_events events_sysmem = []() {
      pw_stream_events e{};
      e.version = PW_VERSION_STREAM_EVENTS;
      e.state_changed = &PipewireProducer::on_state_changed;
      e.param_changed = &PipewireProducer::on_param_changed;
      e.process = &PipewireProducer::on_process;
      return e;
    }();
#if defined(SCORE_PIPEWIRE_OUT_DMABUF)
    static const pw_stream_events events_dmabuf_vk = []() {
      pw_stream_events e{};
      e.version = PW_VERSION_STREAM_EVENTS;
      e.state_changed = &PipewireProducer::on_state_changed;
      e.param_changed = &PipewireProducer::on_param_changed;
      e.add_buffer = &PipewireProducer::on_add_buffer;
      e.remove_buffer = &PipewireProducer::on_remove_buffer;
      return e;
    }();
#endif
#if defined(SCORE_PIPEWIRE_OUT_DMABUF_EGL)
    static const pw_stream_events events_dmabuf_egl = []() {
      pw_stream_events e{};
      e.version = PW_VERSION_STREAM_EVENTS;
      e.state_changed = &PipewireProducer::on_state_changed;
      e.param_changed = &PipewireProducer::on_param_changed;
      e.add_buffer = &PipewireProducer::on_add_buffer_egl;
      e.remove_buffer = &PipewireProducer::on_remove_buffer_egl;
      return e;
    }();
#endif
    const pw_stream_events* events = &events_sysmem;
#if defined(SCORE_PIPEWIRE_OUT_DMABUF) || defined(SCORE_PIPEWIRE_OUT_DMABUF_EGL)
    switch(m_dmabufBackend)
    {
#if defined(SCORE_PIPEWIRE_OUT_DMABUF)
      case DmaBufBackend::Vulkan: events = &events_dmabuf_vk; break;
#endif
#if defined(SCORE_PIPEWIRE_OUT_DMABUF_EGL)
      case DmaBufBackend::EglGbm: events = &events_dmabuf_egl; break;
#endif
      default: break;
    }
#endif
    pw.stream_add_listener(m_stream, &m_listener, events, this);

    uint8_t buffer[1024];
    spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    // DMA-BUF producers must advertise SPA_FORMAT_VIDEO_modifier so the consumer
    // knows what tiling to expect; strict consumers (recent Mutter, sway,
    // modifier-aware wayland sinks) reject a buffer without it. Allocation is
    // always LINEAR, so offer exactly DRM_FORMAT_MOD_LINEAR (= 0);
    // spa_format_video_raw_build emits the property when SPA_VIDEO_FLAG_MODIFIER is
    // set even for a zero value.
    spa_video_info_raw fmt{};
    fmt.format = formats::toSpa(m_fmt);
    fmt.size = SPA_RECTANGLE((uint32_t)m_width, (uint32_t)m_height);
    // Rational framerate: 29.97 -> 29970/1000, not a truncated 29/1.
    fmt.framerate
        = SPA_FRACTION((uint32_t)std::lround(m_fps * 1000.0), 1000u);
#if defined(SCORE_PIPEWIRE_OUT_DMABUF) || defined(SCORE_PIPEWIRE_OUT_DMABUF_EGL)
    if(m_dmabufBackend != DmaBufBackend::None)
    {
      fmt.flags = SPA_VIDEO_FLAG_MODIFIER;
      fmt.modifier = 0; // DRM_FORMAT_MOD_LINEAR = fourcc_mod_code(NONE, 0) = 0
    }
#endif
    const spa_pod* params[2];
    int nparams = 1;
#if defined(SCORE_PIPEWIRE_OUT_DMABUF) || defined(SCORE_PIPEWIRE_OUT_DMABUF_EGL)
    if(m_dmabufBackend != DmaBufBackend::None)
    {
      // The modifier has to be a CHOICE, not a fixed value, or the consumer has
      // nothing to answer with: pipewire's dma-buf negotiation is a two-step
      // handshake where the consumer picks one modifier out of the offered set
      // and the producer is told which in param_changed. Offering a single
      // fixed value the way spa_format_video_raw_build does makes the server
      // tear the link down mid-negotiation -- the consumer sees "clear format"
      // and then "unconnected", which is what a black OBS source is.
      //
      // The first entry of a CHOICE_ENUM is both the default and the first
      // alternative, so LINEAR appears twice on purpose.
      spa_pod_frame f[2];
      spa_pod_builder_push_object(
          &b, &f[0], SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat);
      spa_pod_builder_add(
          &b, SPA_FORMAT_mediaType, SPA_POD_Id(SPA_MEDIA_TYPE_video),
          SPA_FORMAT_mediaSubtype, SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw),
          SPA_FORMAT_VIDEO_format, SPA_POD_Id(fmt.format), 0);

      spa_pod_builder_prop(
          &b, SPA_FORMAT_VIDEO_modifier,
          SPA_POD_PROP_FLAG_MANDATORY | SPA_POD_PROP_FLAG_DONT_FIXATE);
      spa_pod_builder_push_choice(&b, &f[1], SPA_CHOICE_Enum, 0);
      // The driver's own shareable layouts first, then INVALID, then LINEAR.
      // Order is preference: a modifier-tiled export is written far faster
      // than a LINEAR one in host memory, so the tiled layouts go first and
      // the universally-importable ones stay at the back as the fallback for a
      // consumer that cannot take them -- which is what a second GPU is.
      //
      // The first entry is both the default and an alternative, hence the
      // repeat, and INVALID means "implicit, ask the driver", which is what a
      // consumer that does not negotiate explicit modifiers imports.
      const uint64_t first
          = m_drmModifiers.empty() ? uint64_t(kDrmModifierInvalid)
                                   : m_drmModifiers.front();
      spa_pod_builder_long(&b, int64_t(first));
      for(uint64_t m : m_drmModifiers)
        spa_pod_builder_long(&b, int64_t(m));
      spa_pod_builder_long(&b, kDrmModifierInvalid);
      spa_pod_builder_long(&b, 0); // DRM_FORMAT_MOD_LINEAR
      spa_pod_builder_pop(&b, &f[1]);

      spa_pod_builder_add(
          &b, SPA_FORMAT_VIDEO_size, SPA_POD_Rectangle(&fmt.size),
          SPA_FORMAT_VIDEO_framerate, SPA_POD_Fraction(&fmt.framerate), 0);
      params[0] = (const spa_pod*)spa_pod_builder_pop(&b, &f[0]);

      // ... and the same format again WITHOUT a modifier, as a second
      // alternative. A consumer that cannot import our dma-buf otherwise has
      // nothing left to agree on: the negotiation ends in "no more output
      // formats", the stream errors, and the source is black. With this it
      // picks host memory instead and gets a picture. Offering it costs the
      // consumers that CAN do dma-buf nothing -- score's own input still
      // negotiates the dma-buf alternative and still gets it zero-copy.
      spa_video_info_raw plain = fmt;
      plain.flags = 0;
      plain.modifier = 0;
      params[1] = spa_format_video_raw_build(&b, SPA_PARAM_EnumFormat, &plain);
      nparams = 2;
    }
    else
#endif
    {
      params[0] = spa_format_video_raw_build(&b, SPA_PARAM_EnumFormat, &fmt);
    }

    // Passive producer: pipewire pulls frames when the consumer is ready.
    // PW_STREAM_FLAG_DRIVER would make us the timing master, but score drives its
    // own render tick, so PipewireOutputNode::render() publishes the newest frame
    // and on_process hands it to whatever buffer the graph offers. Without
    // DRIVER, AUTOCONNECT does not require an immediate target: the stream sits
    // in CONNECTING / PAUSED until a consumer subscribes.
    //
    // What used to fail here, and what fixed it, is in on_param_changed and
    // on_process. Short version: the stream announced no SPA_PARAM_Buffers, so
    // the server allocated buffers of size zero and every outside consumer
    // dropped what it received. Covered by
    //   PipewireRoundtrip --only s2gst
    //
    // DMA-BUF mode uses PW_STREAM_FLAG_ALLOC_BUFFERS, so pipewire calls back
    // through add_buffer / remove_buffer and we stamp an exportable VkImage's FD
    // and size into each spa_buffer. Sysmem mode uses PW_STREAM_FLAG_MAP_BUFFERS,
    // letting pipewire allocate server-side SHM that push_frame memcpys into.
    const bool useDmaBuf =
#if defined(SCORE_PIPEWIRE_OUT_DMABUF) || defined(SCORE_PIPEWIRE_OUT_DMABUF_EGL)
        m_dmabufBackend != DmaBufBackend::None;
#else
        false;
#endif
    const auto flags = useDmaBuf
                           ? (pw_stream_flags)(PW_STREAM_FLAG_AUTOCONNECT
                                              | PW_STREAM_FLAG_ALLOC_BUFFERS)
                           : (pw_stream_flags)(PW_STREAM_FLAG_AUTOCONNECT
                                              | PW_STREAM_FLAG_MAP_BUFFERS);
    const int rc = pw.stream_connect(
        m_stream, PW_DIRECTION_OUTPUT, PW_ID_ANY, flags, params, nparams);

    pw.thread_loop_unlock(lp);

    if(rc < 0)
    {
      stop();
      return false;
    }
    return true;
  }

  void stop() noexcept
  {
    auto& pw = libremidi::pipewire::load();
    if(!pw.stream_available)
    {
      m_shared.reset();
      return;
    }
    // Destroy under the thread-loop lock: the nested blocking data-loop
    // invoke inside stream_disconnect releases/retakes our lock via the
    // loop-control hooks, which is exactly the documented contract.
    if(auto* lp = loop())
    {
      pw.thread_loop_lock(lp);
      if(m_stream)
      {
        pw.stream_destroy(m_stream);
        m_stream = nullptr;
      }
      pw.thread_loop_unlock(lp);
    }
    // Drop our reference to the shared context. The thread_loop and
    // context+core live on for other consumers (audio, MIDI, input).
    m_shared.reset();
  }

  /** Push a frame's worth of RGBA8 bytes into the next available
   *  pipewire buffer. `size` must be `width*height*4` (we always
   *  produce tightly-packed RGBA at the configured geometry).
   *  Returns true on success, false if pipewire is out of buffers
   *  (typical: consumer didn't release them yet — frame is dropped). */
  bool push_frame(const uint8_t* data, std::size_t size) noexcept
  {
    if(!m_stream || !data || size == 0)
      return false;
    auto& pw = libremidi::pipewire::load();
    if(!pw.stream_available || !m_streaming.load(std::memory_order_acquire))
      return false;

    // Publish, do not queue: on_process fills whatever buffer the graph hands
    // it from here. score renders on its own clock and the consumer is
    // scheduled on the graph's, so the newest frame wins and a consumer slower
    // than score simply skips the ones in between.
    {
      std::lock_guard l{m_stagingMutex};
      m_staging.resize(size);
      std::memcpy(m_staging.data(), data, size);
    }
    return true;
  }

private:
  static void on_state_changed(
      void* self_, pw_stream_state /*old*/, pw_stream_state state,
      const char* error)
  {
    // pw_stream_dequeue_buffer dereferences stream internals that only exist
    // once the stream is streaming; calling it on an UNCONNECTED or ERROR
    // stream faults inside pipewire.
    if(auto* self = static_cast<PipewireProducer*>(self_))
      self->m_streaming.store(
          state == PW_STREAM_STATE_STREAMING, std::memory_order_release);

    auto& pw = libremidi::pipewire::load();
    const char* s = pw.stream_state_as_string
                        ? pw.stream_state_as_string(state)
                        : "(state)";
    qDebug() << "PipeWire output stream state:" << s
             << (error ? error : "");
  }

  /** pipewire pulls: the node is scheduled by the graph driver, and THIS is
   *  where a buffer may be dequeued, filled and queued.
   *
   *  Queueing from score's render thread instead -- dequeue, memcpy, queue,
   *  under the thread-loop lock -- puts the buffer on the stream's queued list
   *  and there it stayed: score reported hundreds of frames queued while
   *  GStreamer's pipewiresrc, linked and negotiated, received none. Filling
   *  the buffer the graph just handed us is the shape every pipewire video
   *  producer uses, and the one an outside consumer actually receives.
   *
   *  Runs on the data thread, already inside the loop: no thread_loop_lock
   *  here, that would deadlock.
   */
  static void on_process(void* self_)
  {
    auto* self = static_cast<PipewireProducer*>(self_);
    if(!self || !self->m_stream)
      return;
    auto& pw = libremidi::pipewire::load();
    if(!pw.stream_available)
      return;

    pw_buffer* b = pw.stream_dequeue_buffer(self->m_stream);
    if(!b)
      return;

    spa_buffer* buf = b->buffer;
    auto* dst = static_cast<uint8_t*>(buf->datas[0].data);
    if(!dst)
    {
      pw.stream_queue_buffer(self->m_stream, b);
      return;
    }

    std::size_t copied = 0;
    {
      std::lock_guard l{self->m_stagingMutex};
      if(!self->m_staging.empty())
      {
        copied = std::min<std::size_t>(
            self->m_staging.size(), buf->datas[0].maxsize);
        std::memcpy(dst, self->m_staging.data(), copied);
      }
    }

    auto* chunk = buf->datas[0].chunk;
    chunk->offset = 0;
    chunk->size = uint32_t(copied);
    chunk->stride = int32_t(self->m_width * self->m_bpp);

    pw.stream_queue_buffer(self->m_stream, b);
    if(copied > 0)
      self->m_framesQueued.fetch_add(1, std::memory_order_relaxed);
  }

  static void
  on_param_changed(void* self_, uint32_t id, const spa_pod* param)
  {
    if(!param || id != SPA_PARAM_Format)
      return;
    qDebug() << "PipeWire output stream format negotiated";

    auto* self = static_cast<PipewireProducer*>(self_);
    if(!self || !self->m_stream)
      return;
    auto& pw = libremidi::pipewire::load();
    if(!pw.stream_available || !pw.stream_update_params)
      return;

    // Which MEMORY the buffers are made of is negotiated here too, and it is
    // the difference between zero-copy and nothing at all. A consumer that
    // asks for video/x-raw(memory:DMABuf) gets "not-negotiated" unless this
    // says SPA_DATA_DmaBuf; a consumer reading host memory needs MemPtr.
    // Announcing the wrong one is not a fallback, it is a dead link.
    uint32_t dataTypes = (1u << SPA_DATA_MemPtr) | (1u << SPA_DATA_MemFd);
#if defined(SCORE_PIPEWIRE_OUT_DMABUF) || defined(SCORE_PIPEWIRE_OUT_DMABUF_EGL)
    if(self->m_dmabufBackend != DmaBufBackend::None)
    {
      dataTypes = (1u << SPA_DATA_DmaBuf);

      // Second half of the dma-buf handshake. We offered the modifier as a
      // DONT_FIXATE choice, so the format that comes back still carries a
      // choice: the consumer has said which modifiers IT can import and it is
      // now our turn to pick one and re-announce the format fixed to it.
      // Skipping this step is what left the link half-negotiated -- the
      // consumer saw "clear format" and then "unconnected", which is a black
      // source in OBS and "not-negotiated" in a GStreamer pipeline.
      if(const spa_pod_prop* mod
         = spa_pod_find_prop(param, nullptr, SPA_FORMAT_VIDEO_modifier);
         mod && (mod->flags & SPA_POD_PROP_FLAG_DONT_FIXATE))
      {
        spa_video_info_raw got{};
        spa_format_video_raw_parse(param, &got);

        // Which modifier to fix on: the reply's choice is the INTERSECTION of
        // what we offered and what the consumer can import, so take its first
        // value rather than assuming ours survived. An empty intersection is
        // the case where dma-buf cannot happen at all with this consumer.
        uint64_t chosenModifier = 0;
        {
          const auto* vals = (const uint64_t*)SPA_POD_CHOICE_VALUES(&mod->value);
          const uint32_t n = SPA_POD_CHOICE_N_VALUES(&mod->value);
          if(n > 0)
            chosenModifier = vals[0];
        }
        // What on_add_buffer will create the exported images with.
        self->m_chosenModifier = chosenModifier;

        uint8_t fixbuf[1024];
        spa_pod_builder fb = SPA_POD_BUILDER_INIT(fixbuf, sizeof(fixbuf));
        spa_pod_frame ff;
        spa_pod_builder_push_object(
            &fb, &ff, SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat);
        spa_pod_builder_add(
            &fb, SPA_FORMAT_mediaType, SPA_POD_Id(SPA_MEDIA_TYPE_video),
            SPA_FORMAT_mediaSubtype, SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw),
            SPA_FORMAT_VIDEO_format, SPA_POD_Id(got.format), 0);
        // MANDATORY, and written through spa_pod_builder_prop: a modifier
        // handed to spa_pod_builder_add carries no flags, and the server reads
        // an unflagged modifier as "not really required" and drops it from the
        // intersection -- which leaves no format at all. This is the shape
        // pipewire's own fixate_format() writes.
        spa_pod_builder_prop(
            &fb, SPA_FORMAT_VIDEO_modifier, SPA_POD_PROP_FLAG_MANDATORY);
        spa_pod_builder_long(&fb, chosenModifier);
        spa_pod_builder_add(
            &fb, SPA_FORMAT_VIDEO_size, SPA_POD_Rectangle(&got.size),
            SPA_FORMAT_VIDEO_framerate, SPA_POD_Fraction(&got.framerate), 0);
        const spa_pod* fixated
            = (const spa_pod*)spa_pod_builder_pop(&fb, &ff);

        // Both, and in this order: the fixated format first, the unfixated
        // offer after it. Announcing only the fixated one leaves the server
        // with nothing to fall back on and it gives up with "no more output
        // formats" -- the same two-entry answer pipewire's own
        // video-src-fixate example sends.
        spa_pod_frame af;
        spa_pod_builder_push_object(
            &fb, &af, SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat);
        spa_pod_builder_add(
            &fb, SPA_FORMAT_mediaType, SPA_POD_Id(SPA_MEDIA_TYPE_video),
            SPA_FORMAT_mediaSubtype, SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw),
            SPA_FORMAT_VIDEO_format, SPA_POD_Id(got.format), 0);
        spa_pod_builder_prop(
            &fb, SPA_FORMAT_VIDEO_modifier,
            SPA_POD_PROP_FLAG_MANDATORY | SPA_POD_PROP_FLAG_DONT_FIXATE);
        spa_pod_frame ac;
        spa_pod_builder_push_choice(&fb, &ac, SPA_CHOICE_Enum, 0);
        const uint64_t altFirst
            = self->m_drmModifiers.empty() ? uint64_t(kDrmModifierInvalid)
                                           : self->m_drmModifiers.front();
        spa_pod_builder_long(&fb, int64_t(altFirst));
        for(uint64_t m : self->m_drmModifiers)
          spa_pod_builder_long(&fb, int64_t(m));
        spa_pod_builder_long(&fb, kDrmModifierInvalid);
        spa_pod_builder_long(&fb, 0);
        spa_pod_builder_pop(&fb, &ac);
        spa_pod_builder_add(
            &fb, SPA_FORMAT_VIDEO_size, SPA_POD_Rectangle(&got.size),
            SPA_FORMAT_VIDEO_framerate, SPA_POD_Fraction(&got.framerate), 0);
        const spa_pod* alternatives
            = (const spa_pod*)spa_pod_builder_pop(&fb, &af);

        const spa_pod* fixParams[2]{fixated, alternatives};
        pw.stream_update_params(self->m_stream, fixParams, 2);
        return; // the buffers come with the next param_changed, once fixed
      }
    }
#endif

    // Answer the negotiated format with the buffers it implies. Without this
    // the server has no size to allocate and hands out spa_buffers whose
    // maxsize is 0: score filled 0 bytes into every one of them, marked the
    // chunk 0 bytes long and queued it, and an outside consumer -- GStreamer's
    // pipewiresrc, OBS -- dropped the lot. It looked like frames never
    // arriving; they arrived empty. score's own input never noticed because
    // the two sides negotiate a size between them.
    const int stride = int(self->m_width * self->m_bpp);
    const int size = stride * int(self->m_height);
    if(stride <= 0 || size <= 0)
      return;

    uint8_t buffer[1024];
    spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    const spa_pod* params[1];
    params[0] = (const spa_pod*)spa_pod_builder_add_object(
        &b, SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers,
        SPA_PARAM_BUFFERS_buffers, SPA_POD_CHOICE_RANGE_Int(4, 2, 16),
        SPA_PARAM_BUFFERS_blocks, SPA_POD_Int(1), SPA_PARAM_BUFFERS_size,
        SPA_POD_Int(size), SPA_PARAM_BUFFERS_stride, SPA_POD_Int(stride),
        SPA_PARAM_BUFFERS_dataType, SPA_POD_CHOICE_FLAGS_Int(dataTypes));

    pw.stream_update_params(self->m_stream, params, 1);
  }

#if defined(SCORE_PIPEWIRE_OUT_DMABUF)
  /** Pipewire is asking us to provide a new buffer for the pool.
   *  Allocate an exportable VkImage matching the configured
   *  width/height/format, export the DMA-BUF FD, and stamp it into
   *  the spa_buffer. */
  static void on_add_buffer(void* opaque, pw_buffer* b)
  {
    auto* self = static_cast<PipewireProducer*>(opaque);
    if(self->m_dmabufBackend != DmaBufBackend::Vulkan
       || !b || !b->buffer || b->buffer->n_datas < 1)
      return;

    score::gfx::vkinterop::ExternalImageDesc desc{};
    desc.format = self->vkFormatFromTag(self->m_fmt);
    desc.extent = {uint32_t(self->m_width), uint32_t(self->m_height), 1};
    desc.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                 | VK_IMAGE_USAGE_TRANSFER_DST_BIT
                 | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                 | VK_IMAGE_USAGE_SAMPLED_BIT;
    // The layout the consumer agreed to, when it agreed to a real one. A
    // modifier-tiled export is written far faster than LINEAR in host memory;
    // LINEAR is what is left when the consumer could only take the implicit
    // modifier, which is the cross-GPU case.
    desc.tiling = VK_IMAGE_TILING_LINEAR;
    const uint64_t chosen = self->m_chosenModifier;
    if(chosen != uint64_t(kDrmModifierInvalid))
    {
      // The consumer named a layout: give it exactly that one.
      desc.drmModifiers = &self->m_chosenModifier;
      desc.drmModifierCount = 1;
    }
    // If the consumer took the IMPLICIT modifier we must stay LINEAR. Letting
    // the driver pick its best layout and calling it "implicit" was tried: the
    // rate did not move and the picture came back wrong -- the importer read a
    // tiled buffer as if it were linear
    // and the gradient turned into three flat values. Implicit means the
    // importer will not ask, so it has to be a layout it can assume.
    desc.handleType
        = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
    desc.dedicated = true;
    // NOT device-local, and this was measured both ways. Asking for
    // device-local moves an 8K frame out of host memory and a GStreamer
    // consumer gains for it -- 10.4 to 15.4 buffers/s. score's own PipeWire
    // input loses far more: 27.7 fps at 36 ms latency becomes 11.7 fps at
    // 425 ms, a fifty-fold latency regression on the one path that was
    // already zero-copy end to end. Until the two can be told apart at
    // allocation time, the working path wins.
    desc.preferDeviceLocal = false;

    auto extImg = score::gfx::vkinterop::createExportableImage(
        self->m_vk, desc);
    if(!extImg)
    {
      qWarning() << "PipewireProducer: createExportableImage failed";
      return;
    }
    auto h = score::gfx::vkinterop::exportMemoryHandle(
        self->m_vk, extImg->memory,
        VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT);
    if(!h)
    {
      score::gfx::vkinterop::destroyExternal(self->m_vk, *extImg);
      qWarning() << "PipewireProducer: exportMemoryHandle failed";
      return;
    }

    DmaBufSlot slot;
    slot.img = *extImg;
    slot.fd = h->fd;
    // LINEAR images are routinely pitch-padded by the driver (alignment,
    // odd widths); consumers must be told the real row pitch, not
    // width*bpp, or the image shears.
    if(auto* df = self->m_vk.qInst
                      ? self->m_vk.qInst->deviceFunctions(self->m_vk.dev)
                      : nullptr)
    {
      VkImageSubresource sub{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0};
      VkSubresourceLayout layout{};
      df->vkGetImageSubresourceLayout(
          self->m_vk.dev, extImg->image, &sub, &layout);
      slot.rowPitch = uint32_t(layout.rowPitch);
    }
    self->m_dmabufSlots[b] = slot;

    auto& d = b->buffer->datas[0];
    d.type = SPA_DATA_DmaBuf;
    d.fd = h->fd;
    d.maxsize = uint32_t(extImg->size);
    d.mapoffset = 0;
    d.data = nullptr; // DMA-BUF: no host mapping, GPU access only
  }

  static void on_remove_buffer(void* opaque, pw_buffer* b)
  {
    auto* self = static_cast<PipewireProducer*>(opaque);
    auto it = self->m_dmabufSlots.find(b);
    if(it == self->m_dmabufSlots.end())
      return;
    if(b == self->m_inFlight)
    {
      // The render thread is writing into this buffer's image with the
      // loop lock released; destroying the VkImage now would fault the
      // in-flight frame. Park the resources for dmabuf_queue.
      self->m_inFlightDeferred = it->second;
      self->m_inFlightRemoved = true;
      self->m_dmabufSlots.erase(it);
      return;
    }
    if(it->second.fd >= 0)
      ::close(it->second.fd);
    score::gfx::vkinterop::destroyExternal(self->m_vk, it->second.img);
    self->m_dmabufSlots.erase(it);
  }
#endif

#if defined(SCORE_PIPEWIRE_OUT_DMABUF_EGL)
  /** EGL/GBM variant of on_add_buffer. Runs on the pipewire thread:
   *  GBM allocation is thread-safe so we do it here, but EGL import +
   *  GL texture creation must happen on the render thread → we mark
   *  needs_import=true and let process_pending_egl_gl handle it on
   *  the next render() under the loop lock. */
  static void on_add_buffer_egl(void* opaque, pw_buffer* b)
  {
    auto* self = static_cast<PipewireProducer*>(opaque);
    if(self->m_dmabufBackend != DmaBufBackend::EglGbm
       || !b || !b->buffer || b->buffer->n_datas < 1)
      return;

    DmaBufEglSlot slot;
    if(!self->m_gbmExporter.allocSlotGbmOnly(
           slot.slot, uint32_t(self->m_width), uint32_t(self->m_height),
           self->m_drmFourcc))
    {
      qWarning() << "PipewireProducer: GBM allocation failed";
      return;
    }
    slot.needs_import = true;
    slot.ready = false;

    auto& d = b->buffer->datas[0];
    d.type = SPA_DATA_DmaBuf;
    d.fd = slot.slot.fd;
    d.maxsize = uint32_t(slot.slot.size);
    d.mapoffset = 0;
    d.data = nullptr;

    self->m_eglSlots[b] = slot;
  }

  static void on_remove_buffer_egl(void* opaque, pw_buffer* b)
  {
    auto* self = static_cast<PipewireProducer*>(opaque);
    auto it = self->m_eglSlots.find(b);
    if(it == self->m_eglSlots.end())
      return;
    // Release GBM-side resources on this thread (GBM is thread-safe).
    // The GL/EGL resources hold their own refcount on the dmabuf via
    // the imported EGLImage, so closing the FD + destroying the gbm_bo
    // here doesn't pull the rug out from under the still-live EGLImage.
    if(it->second.slot.fd >= 0)
    {
      ::close(it->second.slot.fd);
      it->second.slot.fd = -1;
    }
    if(it->second.slot.bo && self->m_gbmExporter.m_bo_destroy)
    {
      self->m_gbmExporter.m_bo_destroy(it->second.slot.bo);
      it->second.slot.bo = nullptr;
    }
    // GL/EGL cleanup is deferred to the render thread.
    it->second.needs_cleanup = true;
    it->second.ready = false;
  }

#endif

#if defined(SCORE_PIPEWIRE_OUT_DMABUF)
  static VkFormat vkFormatFromTag(formats::Tag tag) noexcept
  {
    switch(tag)
    {
      case formats::Tag::RGBA8:    return VK_FORMAT_R8G8B8A8_UNORM;
      case formats::Tag::BGRA8:    return VK_FORMAT_B8G8R8A8_UNORM;
      case formats::Tag::RGB10A2:  return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
      case formats::Tag::BGR10A2:  return VK_FORMAT_A2R10G10B10_UNORM_PACK32;
      case formats::Tag::RGBA16F:  return VK_FORMAT_R16G16B16A16_SFLOAT;
      case formats::Tag::RGBA32F:  return VK_FORMAT_R32G32B32A32_SFLOAT;
      default:                     return VK_FORMAT_R8G8B8A8_UNORM;
    }
  }
#endif

  int m_width{};
  int m_height{};
  double m_fps{};
  formats::Tag m_fmt{formats::Tag::RGBA8};
  int m_bpp{4};
  QString m_nodeName;

  // Process-wide shared pipewire connection. Acquired lazily in
  // start(). The thread_loop, pw_context and pw_core live here.
  std::shared_ptr<libremidi::pipewire::context> m_shared;
  pw_stream* m_stream{};
  std::atomic_bool m_streaming{false};
  spa_hook m_listener{};

public:
  /** Buffers actually handed to pipewire, over every publish path. */
  std::atomic_int m_framesQueued{0};

  //! True once the consumer agreed to an explicit, tiled layout -- the only
  //! case where writing into the shared image directly is worth it.
  bool exported_layout_is_tiled() const noexcept
  {
    return m_chosenModifier != uint64_t(kDrmModifierInvalid);
  }

  //! Layouts we can export, best first, and the one the consumer settled on.
  std::vector<uint64_t> m_drmModifiers;
  uint64_t m_chosenModifier{uint64_t(kDrmModifierInvalid)};

  //! The newest rendered frame, waiting for the graph to ask for one.
  std::mutex m_stagingMutex;
  std::vector<uint8_t> m_staging;

private:

#if defined(SCORE_PIPEWIRE_OUT_DMABUF)
  score::gfx::vkinterop::VulkanCtx m_vk{};
  std::unordered_map<pw_buffer*, DmaBufSlot> m_dmabufSlots;

  // Buffer checked out by the render thread between dmabuf_dequeue and
  // dmabuf_queue. The loop lock is RELEASED during that window (so the
  // pipewire data thread isn't stalled by the multi-ms GPU wait inside
  // endOffscreenFrame); if the server removes the buffer meanwhile,
  // on_remove_buffer parks its resources here and dmabuf_queue destroys
  // them instead of queueing the now-dangling pw_buffer.
  pw_buffer* m_inFlight{nullptr};
  DmaBufSlot m_inFlightDeferred{};
  bool m_inFlightRemoved{false};
#endif

#if defined(SCORE_PIPEWIRE_OUT_DMABUF) || defined(SCORE_PIPEWIRE_OUT_DMABUF_EGL)
  DmaBufBackend m_dmabufBackend{DmaBufBackend::None};
#endif

#if defined(SCORE_PIPEWIRE_OUT_DMABUF_EGL)
public:
  // Public so on_add_buffer_egl / on_remove_buffer_egl (themselves
  // static member functions) can reach members of another instance.
  score::gfx::GbmDmaBufExport m_gbmExporter{};
  score::gfx::EglDmaBufImporter* m_eglImporter{nullptr};
  uint32_t m_drmFourcc{0};
  std::unordered_map<pw_buffer*, DmaBufEglSlot> m_eglSlots;
#endif
};

// ============================================================================
// PwWireRenderer -- InvertYRenderer variant that creates its readback target
// with the WIRE pixel format rather than renderer.state.renderFormat, which is
// always RGBA8, and only flips Y on a Y-up framebuffer (GL). score's shaders
// apply clipSpaceCorrMatrix, so on Vulkan, D3D and Metal the rendered texture
// is already top-down.
// ============================================================================
struct PwWireRenderer final : score::gfx::OutputNodeRenderer
{
  PwWireRenderer(
      const score::gfx::Node& n, score::gfx::TextureRenderTarget rt,
      QRhiReadbackResult& readback, QRhiTexture::Format fmt)
      : score::gfx::OutputNodeRenderer{n}
      , m_inputTarget{std::move(rt)}
      , m_wireFormat{fmt}
      , m_readback{&readback}
  {
  }

  score::gfx::TextureRenderTarget m_inputTarget;
  score::gfx::TextureRenderTarget m_renderTarget;
  score::gfx::TextureRenderTarget m_externalTarget{};
  QRhiTexture::Format m_wireFormat{QRhiTexture::RGBA8};
  QShader m_vertexS, m_fragmentS;
  std::vector<score::gfx::Sampler> m_samplers;
  score::gfx::Pipeline m_p;
  score::gfx::MeshBuffers m_mesh{};

  score::gfx::TextureRenderTarget
  renderTargetForInput(const score::gfx::Port&) override
  {
    return m_inputTarget;
  }

  /** The flip-corrected, wire-format texture — what actually goes on the
   *  wire. The DMA-BUF publish path must copy from THIS (not the upstream
   *  input texture): on GL the upstream render is Y-up, and only this
   *  pass applies the orientation correction. */
  QRhiTexture* wireTexture() const noexcept { return m_renderTarget.texture; }

  /** Draw this frame's wire pass straight into \p rt instead of into our own
   *  texture. Empty means "use my own".
   *
   *  This is what makes the hand-over a true share rather than a copy: the
   *  final pass writes into the pipewire buffer the consumer will import, so
   *  the frame is produced once and read once, the way Spout and Syphon work.
   */
  void setExternalTarget(const score::gfx::TextureRenderTarget& rt) noexcept
  {
    m_externalTarget = rt;
  }

  void init(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res) override
  {
    m_renderTarget = score::gfx::createRenderTarget(
        renderer.state, m_wireFormat, m_inputTarget.texture->pixelSize(),
        renderer.samples(), renderer.requiresDepth(*this->node.input[0]));

    const auto& mesh = renderer.defaultTriangle();
    m_mesh = renderer.initMeshBuffer(mesh, res);

    // The same correction Gfx::InvertYRenderer applies, and for the same
    // reason: this pass draws the engine's default triangle, whose vertex
    // shader puts the geometry through renderer.clipSpaceCorrMatrix, so what
    // it needs is decided by that and not by QRhi::isYUpInFramebuffer().
    // Deciding it on the framebuffer origin left this pass without a flip on
    // Vulkan and put an upside-down picture on the wire -- which the
    // score-to-score round trip could not see, because the TexgenNode it
    // painted with was upside down on Vulkan too.
    static const constexpr auto flip_filter = R"_(#version 450
    layout(location = 0) in vec2 v_texcoord;
    layout(location = 0) out vec4 fragColor;
    layout(binding = 3) uniform sampler2D tex;
    void main()
    {
#if defined(QSHADER_MSL) || defined(QSHADER_HLSL)
      fragColor = texture(tex, v_texcoord);
#else
      fragColor = texture(tex, vec2(v_texcoord.x, 1. - v_texcoord.y));
#endif
    }
    )_";
    std::tie(m_vertexS, m_fragmentS) = score::gfx::makeShaders(
        renderer.state, mesh.defaultVertexShader(), flip_filter);

    auto sampler = renderer.state.rhi->newSampler(
        QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
        QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
    sampler->setName("PwWireRenderer::sampler");
    sampler->create();
    m_samplers.push_back({sampler, this->m_inputTarget.texture});

    m_p = score::gfx::buildPipeline(
        renderer, mesh, m_vertexS, m_fragmentS, m_renderTarget, nullptr,
        nullptr, m_samplers);
  }

  void update(
      score::gfx::RenderList&, QRhiResourceUpdateBatch&, score::gfx::Edge*) override
  {
  }

  void release(score::gfx::RenderList&) override
  {
    m_p.release();
    for(auto& s : m_samplers)
      delete s.sampler;
    m_samplers.clear();
    m_renderTarget.release();
  }

  void finishFrame(
      score::gfx::RenderList& renderer, QRhiCommandBuffer& cb,
      QRhiResourceUpdateBatch*& res) override
  {
    auto* target = m_externalTarget.renderTarget ? m_externalTarget.renderTarget
                                                 : m_renderTarget.renderTarget;
    cb.beginPass(target, Qt::black, {0.0f, 0}, res);
    res = nullptr;
    {
      const auto sz = renderer.state.renderSize;
      cb.setGraphicsPipeline(m_p.pipeline);
      cb.setShaderResources(m_p.srb);
      cb.setViewport(QRhiViewport(0, 0, sz.width(), sz.height()));
      const auto& mesh = renderer.defaultTriangle();
      mesh.draw(this->m_mesh, cb);
    }
    // The readback is for the SYSMEM path only: push_frame copies out of
    // m_readback. In dma-buf mode nothing ever reads it, and enqueuing it
    // anyway costs a full frame pulled back across the bus plus a memcpy on
    // the CPU -- most of an 8K frame, measured with perf, while the render
    // itself was a fraction of a millisecond.
    if(m_wantReadback && m_readback)
    {
      auto next = renderer.state.rhi->nextResourceUpdateBatch();
      QRhiReadbackDescription rb(m_renderTarget.texture);
      next->readBackTexture(rb, m_readback);
      cb.endPass(next);
    }
    else
    {
      cb.endPass(res);
      res = nullptr;
    }
  }

  //! Off in dma-buf mode: see finishFrame.
  void setReadbackEnabled(bool b) noexcept { m_wantReadback = b; }

private:
  QRhiReadbackResult* m_readback{};
  bool m_wantReadback{true};
};

} // namespace

// ============================================================================
// PipewireOutputNode — score::gfx::OutputNode that drives the producer
// ============================================================================

struct PipewireOutputNode : score::gfx::OutputNode
{
  explicit PipewireOutputNode(const SharedOutputSettings& s);
  ~PipewireOutputNode() override;

  void startRendering() override;
  void onRendererChange() override;
  void render() override;
  bool canRender() const override;
  void stopRendering() override;

  void setRenderer(std::shared_ptr<score::gfx::RenderList> r) override;
  score::gfx::RenderList* renderer() const override;

  void createOutput(score::gfx::OutputConfiguration conf) override;
  void destroyOutput() override;

  std::shared_ptr<score::gfx::RenderState> renderState() const override;
  score::gfx::OutputNodeRenderer*
  createRenderer(score::gfx::RenderList& r) const noexcept override;
  Configuration configuration() const noexcept override;

  std::weak_ptr<score::gfx::RenderList> m_renderer{};
  // Cached by createRenderer (mutable: createRenderer is const). Owned by
  // the render list; refreshed whenever the graph rebuilds renderers.
  mutable PwWireRenderer* m_wireRenderer{};
  QRhiTexture* m_texture{};
  QRhiTextureRenderTarget* m_renderTarget{};
  std::shared_ptr<score::gfx::RenderState> m_renderState{};
  std::unique_ptr<PipewireProducer> m_producer;

  SharedOutputSettings m_settings;
  QRhiReadbackResult m_readback;
  int m_bytesPerPixel{4}; /**< Bytes per pixel of the negotiated format. */
  formats::Tag m_tag{formats::Tag::RGBA8}; /**< Wire pixel format. */
  std::vector<uint8_t> m_repack; /**< Scratch for wire-format repacking. */

#if defined(SCORE_PIPEWIRE_OUT_DMABUF)
  // DMA-BUF output mode (Vulkan): a "bridge" QRhiTexture that wraps the
  // currently-dequeued pipewire buffer's exported VkImage. Per-frame
  // we createFrom this with the right VkImage handle, then issue a
  // QRhi copyTexture from m_texture to m_dmabufBridge.
  bool m_dmabufMode{false};
  QRhiTexture* m_dmabufBridge{};
  //! One QRhi wrapper per exported VkImage, kept for the pool's lifetime.
  //!
  //! createFrom(..., VK_IMAGE_LAYOUT_UNDEFINED) on every frame tells QRhi that
  //! the destination's contents are undefined and its layout unknown, so the
  //! copy re-transitions the whole image each time. At 8K that is 133 MB of
  //! layout work per frame on top of the copy. pipewire rotates a FIXED pool,
  //! so wrapping each image once and reusing the wrapper keeps QRhi's tracked
  //! layout stable and the transition disappears.
  ossia::hash_map<quint64, score::gfx::TextureRenderTarget> m_dmabufTargets;
  score::gfx::vkinterop::VulkanCtx m_vk{};
#endif

#if defined(SCORE_PIPEWIRE_OUT_DMABUF_EGL)
  // DMA-BUF output mode (EGL/GBM): same idea, but the bridge wraps a
  // GL texture id. The producer-owned EglDmaBufImporter is given to
  // the producer via enable_dmabuf_mode_egl(); it does the actual
  // import + glEGLImageTargetTexture2DOES on the render thread.
  bool m_dmabufEglMode{false};
  QRhiTexture* m_dmabufBridgeEgl{};
  score::gfx::EglDmaBufImporter m_eglImporter{};
#endif
};

PipewireOutputNode::PipewireOutputNode(const SharedOutputSettings& s)
    : OutputNode{}
    , m_settings{s}
{
  input.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Image, {}});
}

PipewireOutputNode::~PipewireOutputNode() = default;

bool PipewireOutputNode::canRender() const
{
  return bool(m_producer);
}

void PipewireOutputNode::startRendering() { }
void PipewireOutputNode::onRendererChange() { }
void PipewireOutputNode::stopRendering() { }

void PipewireOutputNode::render()
{
  auto rl = m_renderer.lock();
  if(!rl || !m_renderState || !m_producer)
    return;

  auto* rhi = m_renderState->rhi;

#if defined(SCORE_PIPEWIRE_OUT_DMABUF)
  // -------- DMA-BUF output mode --------------------------------------
  // Dequeue a pipewire buffer first; the producer hands us the
  // associated VkImage. Render score's frame normally to m_texture,
  // then issue a GPU copyTexture from m_texture to the bridge
  // texture (which wraps the dequeued buffer's VkImage). Wait for
  // GPU completion via vkQueueWaitIdle, then queue_buffer.
  if(m_dmabufMode && m_dmabufBridge)
  {
    VkImage targetImage{VK_NULL_HANDLE};
    VkDeviceMemory targetMemory{VK_NULL_HANDLE};
    pw_buffer* pwbuf
        = m_producer->dmabuf_dequeue(&targetImage, &targetMemory);
    if(!pwbuf)
    {
      // No pipewire buffer available (consumer slow / not yet
      // subscribed). Skip this frame — score keeps rendering normally
      // for its other outputs.
      return;
    }

    // Wrap the dequeued buffer's VkImage, once per image and kept: a render
    // target over it as well as a texture, because the wire pass draws INTO
    // the pipewire buffer rather than into a texture we then copy. That is
    // what makes this a share and not a copy -- the same image the consumer
    // imports is the one the last pass writes.
    score::gfx::TextureRenderTarget wireTarget{};
    if(auto it = m_dmabufTargets.find(quint64(targetImage));
       it != m_dmabufTargets.end())
    {
      wireTarget = it->second;
    }
    else
    {
      auto* tex = rhi->newTexture(
          m_dmabufBridge->format(), m_dmabufBridge->pixelSize(), 1,
          QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource);
      if(tex->createFrom(QRhiTexture::NativeTexture{
             quint64(targetImage), VK_IMAGE_LAYOUT_UNDEFINED}))
      {
        auto* rt = rhi->newTextureRenderTarget({tex});
        auto* rp = rt->newCompatibleRenderPassDescriptor();
        rt->setRenderPassDescriptor(rp);
        if(rt->create())
        {
          wireTarget = score::gfx::TextureRenderTarget{
              .texture = tex, .renderPass = rp, .renderTarget = rt};
          m_dmabufTargets.emplace(quint64(targetImage), wireTarget);
        }
        else
        {
          delete rt;
          delete rp;
          delete tex;
        }
      }
      else
      {
        delete tex;
      }
    }
    QRhiTexture* bridge = wireTarget.texture;

    QRhiCommandBuffer* cb{};
    if(rhi->beginOffscreenFrame(&cb) != QRhi::FrameOpSuccess)
    {
      m_producer->dmabuf_queue(pwbuf);
      return;
    }

    // Write into the shared image directly -- the Spout/Syphon shape, one
    // texture written once and read once -- but ONLY when that image has a
    // tiled layout. Rendering into a LINEAR host-visible export is far worse
    // than rendering into an ordinary texture and copying: measured at 8K the
    // direct write is about half the rate of the copy, with 4K unchanged. A
    // bulk transfer copes with linear memory; a fragment shader scattering
    // writes across it does not.
    const bool direct = bool(wireTarget.renderTarget) && m_wireRenderer
                        && m_producer->exported_layout_is_tiled();
    if(direct)
      m_wireRenderer->setExternalTarget(wireTarget);

    rl->render(*cb);

    if(direct)
      m_wireRenderer->setExternalTarget({});

    // Queue the texture-to-texture copy. QRhi inserts the
    // appropriate layout transitions + barriers around vkCmdCopyImage.
    // Copy from the renderer's WIRE texture (orientation-corrected,
    // wire-format — the same texture the readback path verifies), not
    // the upstream render target: copying m_texture directly puts a
    // vertically-flipped raster on the wire.
    if(!direct)
    {
      auto* batch = rhi->nextResourceUpdateBatch();
      QRhiTextureCopyDescription cdesc;
      cdesc.setPixelSize(QSize{m_settings.width, m_settings.height});
      QRhiTexture* wireSrc
          = (m_wireRenderer && m_wireRenderer->wireTexture())
                ? m_wireRenderer->wireTexture()
                : m_texture;
      batch->copyTexture(bridge, wireSrc, cdesc);
      cb->resourceUpdate(batch);
    }

    rhi->endOffscreenFrame();

    // pipewire's `queue_buffer == ready` contract wants the GPU work
    // COMPLETE, not just submitted: consumers mmap-read the dma-buf
    // immediately and would see stale/partial content.
    // QRhi::finish() waits for the queue to drain — the pragmatic
    // correct sync. Upgrade path: VkFence on just this submission, or
    // explicit-sync metadata (SPA_META_SyncTimeline).
    rhi->finish();

    // Hand to pipewire. dmabuf_queue handles releasing the thread-loop
    // lock that dmabuf_dequeue acquired, so the buffer's visible to
    // consumers exactly when this returns.
    m_producer->dmabuf_queue(pwbuf);
    return;
  }
#endif

#if defined(SCORE_PIPEWIRE_OUT_DMABUF_EGL)
  // -------- DMA-BUF output mode (EGL/GBM) ----------------------------
  // Symmetric to the Vulkan path: dequeue a pipewire buffer (which the
  // producer associates with one of its pre-imported GL textures),
  // render normally, copyTexture from m_texture into the bridge, then
  // glFinish() (GL has no offscreen-frame fence) before queue.
  if(m_dmabufEglMode && m_dmabufBridgeEgl)
  {
    unsigned int targetTex{0};
    pw_buffer* pwbuf = m_producer->dmabuf_dequeue_egl(&targetTex);
    if(!pwbuf)
      return;

    // Wrap the dequeued buffer's GL texture as our bridge. QRhi's GL
    // backend wraps an existing texture id via NativeTexture{id, 0}.
    m_dmabufBridgeEgl->createFrom(
        QRhiTexture::NativeTexture{quint64(targetTex), 0});

    QRhiCommandBuffer* cb{};
    if(rhi->beginOffscreenFrame(&cb) != QRhi::FrameOpSuccess)
    {
      m_producer->dmabuf_queue_egl(pwbuf);
      return;
    }

    rl->render(*cb);

    rhi->endOffscreenFrame();

    // Copy the flip-corrected wire texture into the EGLImage-bound target
    // with an explicit framebuffer blit. QRhi's copyTexture path
    // (glCopyImageSubData) does not reliably write into EGLImage-backed
    // external textures on NVIDIA — the copy silently no-ops and the
    // consumer reads black. A two-FBO glBlitFramebuffer goes through the
    // normal raster path and works everywhere.
    QRhiTexture* wireSrc
        = (m_wireRenderer && m_wireRenderer->wireTexture())
              ? m_wireRenderer->wireTexture()
              : m_texture;
    if(auto* ctx = QOpenGLContext::currentContext(); ctx && wireSrc)
    {
      auto* f = ctx->extraFunctions();
      const GLuint srcTex = GLuint(wireSrc->nativeTexture().object);
      const int w = m_settings.width, h = m_settings.height;
      GLuint fbos[2]{};
      f->glGenFramebuffers(2, fbos);
      f->glBindFramebuffer(GL_READ_FRAMEBUFFER, fbos[0]);
      f->glFramebufferTexture2D(
          GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, srcTex, 0);
      f->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbos[1]);
      f->glFramebufferTexture2D(
          GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, targetTex,
          0);
      f->glBlitFramebuffer(
          0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
      f->glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
      f->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
      f->glDeleteFramebuffers(2, fbos);
      // glFinish: consumers may mmap-read the DMA-BUF immediately after
      // the buffer is queued; make sure the blit fully landed. Upgrade
      // path: EGL_KHR_fence_sync + explicit-sync metadata.
      f->glFinish();
      if(const auto err = f->glGetError(); err != 0)
      {
        static bool warned = false;
        if(!warned)
        {
          warned = true;
          qWarning() << "PipewireOutputNode: dmabuf blit GL error" << err;
        }
      }
    }

    m_producer->dmabuf_queue_egl(pwbuf);
    return;
  }
#endif

  // -------- Sysmem readback path (default / non-Vulkan) -------------
  QRhiCommandBuffer* cb{};
  if(rhi->beginOffscreenFrame(&cb) != QRhi::FrameOpSuccess)
    return;

  rl->render(*cb);
  rhi->endOffscreenFrame();

  // m_readback was populated by PwWireRenderer during the frame.
  // QRhi reads back tightly-packed bytes at the texture's actual
  // bit-depth, so the byte count is `w * h * bytesPerPixel(format)`.
  const int sz = m_readback.pixelSize.width()
                 * m_readback.pixelSize.height() * m_bytesPerPixel;
  const int bytes = m_readback.data.size();
  if(bytes > 0 && bytes >= sz)
  {
    const auto* src = reinterpret_cast<const uint8_t*>(m_readback.data.constData());
    // Repack readback bytes to the advertised wire layout where QRhi's memory order
    // differs from the SPA format:
    //  - BGRA8: readback is RGBA-ordered, so swap R and B.
    //  - RGB10A2: QRhi words are A2B10G10R10 with R in bits 0-9, SPA xRGB_210LE
    //    wants R in bits 20-29, so swap the two 10-bit fields.
    //  - BGR10A2 matches SPA xBGR_210LE, and RGBA8 / RGBA16F / RGBA32F are raw.
    switch(m_tag)
    {
      case formats::Tag::BGRA8: {
        m_repack.resize(std::size_t(sz));
        for(int i = 0; i < sz; i += 4)
        {
          m_repack[i + 0] = src[i + 2];
          m_repack[i + 1] = src[i + 1];
          m_repack[i + 2] = src[i + 0];
          m_repack[i + 3] = src[i + 3];
        }
        src = m_repack.data();
        break;
      }
      case formats::Tag::RGB10A2: {
        m_repack.resize(std::size_t(sz));
        const auto* in = reinterpret_cast<const uint32_t*>(src);
        auto* out = reinterpret_cast<uint32_t*>(m_repack.data());
        const int n = sz / 4;
        for(int i = 0; i < n; ++i)
        {
          const uint32_t w = in[i];
          out[i] = (w & 0xC00FFC00u) | ((w & 0x3FFu) << 20) | ((w >> 20) & 0x3FFu);
        }
        src = m_repack.data();
        break;
      }
      default:
        break;
    }
    m_producer->push_frame(src, std::size_t(sz));
  }
}

score::gfx::OutputNode::Configuration
PipewireOutputNode::configuration() const noexcept
{
  return {.manualRenderingRate = 1000. / m_settings.rate};
}

void PipewireOutputNode::setRenderer(std::shared_ptr<score::gfx::RenderList> r)
{
  m_renderer = r;
}

score::gfx::RenderList* PipewireOutputNode::renderer() const
{
  return m_renderer.lock().get();
}

void PipewireOutputNode::createOutput(score::gfx::OutputConfiguration conf)
{
  // Parse URL query: "node-name?format=rgba16f&dmabuf=on".
  QString nodeName = m_settings.path;
  formats::Tag tag = formats::Tag::RGBA8;
  bool wantDmaBuf = false;
  {
    static const QRegularExpression urlRe("([^?]*)(?:\\?(.*))?");
    const auto m = urlRe.match(m_settings.path);
    if(m.hasMatch())
    {
      nodeName = m.captured(1);
      static const QRegularExpression paramRe("([^=&]+)=([^&]+)");
      auto it = paramRe.globalMatch(m.captured(2));
      while(it.hasNext())
      {
        const auto pm = it.next();
        const QString k = pm.captured(1);
        const QString v = pm.captured(2);
        if(k == "format")
        {
          const auto t = formats::tagFromString(v);
          // The output can only produce formats QRhi can render into.
          // Accepting a YUV, planar or packed-YUV tag would push RGBA8 readback
          // bytes under the advertised format. Reject.
          switch(t)
          {
            case formats::Tag::RGBA8:
            case formats::Tag::BGRA8:
            case formats::Tag::RGB10A2:
            case formats::Tag::BGR10A2:
            case formats::Tag::RGBA16F:
            case formats::Tag::RGBA32F:
              tag = t;
              break;
            default:
              qWarning() << "PipewireOutputNode: format" << v
                         << "cannot be produced by the output path — "
                            "falling back to rgba";
              break;
          }
        }
        else if(k == "dmabuf")
        {
          wantDmaBuf = (v == "on" || v == "true" || v == "1");
        }
      }
    }
  }

  // The DMA-BUF paths hand raw GPU texture bytes (R,G,B,A component
  // order) to the consumer with no CPU repack opportunity, so only the
  // RGBA8 wire format is honest there. Other tags fall back to the
  // readback path, which repacks on the CPU.
  if(wantDmaBuf && tag != formats::Tag::RGBA8)
  {
    qWarning() << "PipewireOutputNode: dmabuf=on only supports format=rgba —"
                  " using the CPU readback path for tag" << int(tag);
    wantDmaBuf = false;
  }

  m_tag = tag;
  m_producer = std::make_unique<PipewireProducer>(
      m_settings.width, m_settings.height, m_settings.rate, tag, nodeName);

  m_renderState = score::gfx::createRenderState(
      conf.graphicsApi, QSize(m_settings.width, m_settings.height), nullptr);
  if(!m_renderState || !m_renderState->rhi)
  {
    qWarning() << "PipewireOutputNode: failed to create QRhi";
    m_producer.reset();
    m_renderState.reset();
    return;
  }
  m_renderState->outputSize = m_renderState->renderSize;

  m_bytesPerPixel = int(formats::bytesPerPixel(tag));

  auto* rhi = m_renderState->rhi;
  m_texture = rhi->newTexture(
      formats::toQRhi(tag), m_renderState->renderSize, 1,
      QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource);
  m_texture->create();
  m_renderTarget = rhi->newTextureRenderTarget({m_texture});
  m_renderState->renderPassDescriptor
      = m_renderTarget->newCompatibleRenderPassDescriptor();
  m_renderTarget->setRenderPassDescriptor(m_renderState->renderPassDescriptor);
  m_renderTarget->create();

#if defined(SCORE_PIPEWIRE_OUT_DMABUF)
  // DMA-BUF output mode: requires Vulkan QRhi. Enable on the producer
  // so it allocates exportable VkImages in on_add_buffer. The bridge
  // QRhiTexture is createFrom'd against the dequeued buffer's VkImage
  // each render() call; QRhi's copyTexture in the resource batch
  // moves bytes from m_texture to it (one GPU copy, no CPU readback).
  if(wantDmaBuf && rhi->backend() == QRhi::Vulkan)
  {
    auto* nh
        = static_cast<const QRhiVulkanNativeHandles*>(rhi->nativeHandles());
    if(nh && nh->dev && nh->physDev && nh->inst)
    {
      m_vk.instance = nh->inst->vkInstance();
      m_vk.physDev = nh->physDev;
      m_vk.dev = nh->dev;
      m_vk.qInst = nh->inst;

      // Bridge texture: a QRhi-owned wrapper around (eventually) the
      // pipewire buffer's exported VkImage. We create it now with a
      // placeholder VkImage; render() calls createFrom each frame
      // with the dequeued buffer's image.
      m_dmabufBridge = rhi->newTexture(
          formats::toQRhi(tag), m_renderState->renderSize, 1,
          QRhiTexture::UsedAsTransferSource);
      m_dmabufBridge->create();

      m_producer->enable_dmabuf_mode(m_vk);
      m_dmabufMode = true;
    }
    else
    {
      qWarning() << "PipewireOutputNode: dmabuf=on requested but "
                    "QRhi Vulkan native handles unavailable — "
                    "falling back to readback path";
    }
  }
#endif

#if defined(SCORE_PIPEWIRE_OUT_DMABUF_EGL)
  // DMA-BUF output mode (EGL/GBM), used on the OpenGLES2 backend when the EGL
  // DMA-BUF extensions are available. Mirrors the Vulkan path: producer-side GBM
  // allocation, with a bridge QRhiTexture wrapping the GL texture id the EGL
  // importer re-targets. The two branches are mutually exclusive on QRhi::Vulkan
  // vs QRhi::OpenGLES2.
  //
  // GPU-writability probe: allocate one small BO, import it, and check it can be
  // an FBO colour attachment. On NVIDIA's GBM backend a BO can only be allocated
  // USE_LINEAR, and the resulting EGLImage-bound texture is sample-only, so every
  // GPU write fails with GL_INVALID_OPERATION and the consumer sees black frames.
  // Detect that and fall back to the readback path.
  const auto eglDmabufGpuWritable
      = [](score::gfx::EglDmaBufImporter& imp) noexcept {
    auto* ctx = QOpenGLContext::currentContext();
    if(!ctx)
      return false;
    score::gfx::GbmDmaBufExport ex;
    if(!ex.init())
      return false;
    score::gfx::GbmDmaBufExport::Slot s{};
    bool ok = false;
    if(ex.allocSlot(
           s, 64, 64, score::gfx::GbmDmaBufExport::DRM_FORMAT_ABGR8888_v,
           imp))
    {
      auto* f = ctx->extraFunctions();
      GLuint fbo = 0;
      f->glGenFramebuffers(1, &fbo);
      f->glBindFramebuffer(GL_FRAMEBUFFER, fbo);
      f->glFramebufferTexture2D(
          GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s.glTexture, 0);
      ok = f->glCheckFramebufferStatus(GL_FRAMEBUFFER)
           == GL_FRAMEBUFFER_COMPLETE;
      f->glBindFramebuffer(GL_FRAMEBUFFER, 0);
      f->glDeleteFramebuffers(1, &fbo);
      while(f->glGetError() != 0)
        ; // drain
      ex.destroySlot(s, imp);
    }
    ex.shutdown();
    return ok;
  };

  if(wantDmaBuf && rhi->backend() == QRhi::OpenGLES2)
  {
    if(score::gfx::GbmDmaBufExport::isAvailable()
       && score::gfx::EglDmaBufImporter::isAvailable(*rhi)
       && m_eglImporter.init(*rhi) && eglDmabufGpuWritable(m_eglImporter))
    {
      m_dmabufBridgeEgl = rhi->newTexture(
          formats::toQRhi(tag), m_renderState->renderSize, 1,
          QRhiTexture::UsedAsTransferSource);
      m_dmabufBridgeEgl->create();

      // The GBM BO receives a raw GPU copy of the texture's component
      // bytes, which are R,G,B,A order for every QRhi format we render
      // (GL stores "BGRA8" as RGBA components internally). Per the DRM
      // fourcc spec, [R,G,B,A] memory on LE is DRM_FORMAT_ABGR8888. Since
      // we cannot swizzle on this pure-GPU path, dmabuf mode only supports
      // the RGBA8 wire format (validated in createOutput).
      const uint32_t fourcc
          = score::gfx::GbmDmaBufExport::DRM_FORMAT_ABGR8888_v;

      if(m_producer->enable_dmabuf_mode_egl(&m_eglImporter, fourcc))
      {
        m_dmabufEglMode = true;
      }
      else
      {
        qWarning() << "PipewireOutputNode: enable_dmabuf_mode_egl "
                      "failed — falling back to readback path";
        delete m_dmabufBridgeEgl;
        m_dmabufBridgeEgl = nullptr;
      }
    }
    else
    {
      qWarning() << "PipewireOutputNode: dmabuf=on requested but "
                    "EGL/GBM DMA-BUF unavailable — falling back to "
                    "readback path";
    }
  }
#endif

#if !defined(SCORE_PIPEWIRE_OUT_DMABUF) && !defined(SCORE_PIPEWIRE_OUT_DMABUF_EGL)
  (void)wantDmaBuf;
#endif

  // Connect the producer stream. Must happen AFTER the dmabuf-mode flags
  // above (enable_dmabuf_mode* document "call before start()") — start()
  // builds the EnumFormat/stream events from them.
  if(!m_producer->start())
    qWarning() << "PipewireOutputNode: producer stream failed to start — "
                  "no frames will be published";

  conf.onReady();
}

void PipewireOutputNode::destroyOutput()
{
  // Producer cleanup must run BEFORE the QRhi tear-down so its
  // on_remove_buffer callbacks (which destroy exportable VkImages)
  // run while the Vulkan device is still alive.
  m_producer.reset();
  if(!m_renderState)
    return;
  releaseRegistry();

#if defined(SCORE_PIPEWIRE_OUT_DMABUF)
  if(m_dmabufBridge)
  {
    delete m_dmabufBridge;
    m_dmabufBridge = nullptr;
  }
  m_dmabufMode = false;
#endif

#if defined(SCORE_PIPEWIRE_OUT_DMABUF_EGL)
  if(m_dmabufBridgeEgl)
  {
    delete m_dmabufBridgeEgl;
    m_dmabufBridgeEgl = nullptr;
  }
  m_dmabufEglMode = false;
  // m_eglImporter has no destructor / explicit shutdown — dylib_loader
  // dtor releases libEGL; the m_display + entry pointers are non-owning.
#endif

  delete m_renderTarget;
  m_renderTarget = nullptr;
  delete m_renderState->renderPassDescriptor;
  m_renderState->renderPassDescriptor = nullptr;
  delete m_texture;
  m_texture = nullptr;

  m_renderState->destroy();
  m_renderState.reset();
}

std::shared_ptr<score::gfx::RenderState>
PipewireOutputNode::renderState() const
{
  return m_renderState;
}

score::gfx::OutputNodeRenderer* PipewireOutputNode::createRenderer(
    score::gfx::RenderList& /*r*/) const noexcept
{
  score::gfx::TextureRenderTarget rt{
      .texture = m_texture,
      .renderPass = m_renderState->renderPassDescriptor,
      .renderTarget = m_renderTarget};
  // BGRA8 renders/reads back through an RGBA8 target: GL has no true BGRA
  // storage (QRhi readback yields R,G,B,A component order regardless), so
  // the wire byte order is produced by a CPU swizzle in render() instead.
  const auto rbFormat = m_tag == formats::Tag::BGRA8 ? QRhiTexture::RGBA8
                                                     : formats::toQRhi(m_tag);
  auto* r = new PwWireRenderer{
      *this, rt, const_cast<QRhiReadbackResult&>(m_readback), rbFormat};
  // The DMA-BUF publish path copies from the renderer's wire texture
  // (flip-corrected). Renderer lifetime == render list lifetime; the graph
  // rebuilds it through this same function, so the cached pointer is
  // refreshed with it.
  m_wireRenderer = r;
  // Nothing consumes the readback when the frame is handed over as a dma-buf.
  r->setReadbackEnabled(!(m_dmabufMode || m_dmabufEglMode));
  return r;
}

// ============================================================================
// pipewire_output_device — ossia::net::device_base wrapper
// ============================================================================

class pipewire_output_device : public ossia::net::device_base
{
  gfx_node_base root;

public:
  pipewire_output_device(
      const SharedOutputSettings& set,
      std::unique_ptr<gfx_protocol_base> proto, std::string name)
      : ossia::net::device_base{std::move(proto)}
      , root{
            *this, *static_cast<gfx_protocol_base*>(m_protocol.get()),
            new PipewireOutputNode{set}, std::move(name)}
  {
  }

  const gfx_node_base& get_root_node() const override { return root; }
  gfx_node_base& get_root_node() override { return root; }
};

// ============================================================================
// PipewireOutputDevice — protocol wrapper
// ============================================================================

class PipewireOutputDevice final : public GfxOutputDevice
{
  W_OBJECT(PipewireOutputDevice)
public:
  using GfxOutputDevice::GfxOutputDevice;
  ~PipewireOutputDevice() override;

private:
  void disconnect() override;
  bool reconnect() override;
  ossia::net::device_base* getDevice() const override { return m_dev.get(); }

  gfx_protocol_base* m_protocol{};
  mutable std::unique_ptr<ossia::net::device_base> m_dev;
};

PipewireOutputDevice::~PipewireOutputDevice() = default;

void PipewireOutputDevice::disconnect()
{
  GfxOutputDevice::disconnect();
  auto prev = std::move(m_dev);
  m_dev = {};
  deviceChanged(prev.get(), nullptr);
}

bool PipewireOutputDevice::reconnect()
{
  disconnect();
  try
  {
    const auto set = m_settings.deviceSpecificSettings.value<SharedOutputSettings>();
    auto* plug = m_ctx.findPlugin<Gfx::DocumentPlugin>();
    if(plug)
    {
      auto proto = std::make_unique<gfx_protocol_base>(plug->exec);
      m_protocol = proto.get();
      m_dev = std::make_unique<pipewire_output_device>(
          set, std::move(proto), this->settings().name.toStdString());
      deviceChanged(nullptr, m_dev.get());
    }
  }
  catch(std::exception& e)
  {
    qDebug() << "PipeWire output reconnect failed:" << e.what();
  }
  catch(...)
  {
    qDebug() << "PipeWire output reconnect: unknown error";
  }
  return connected();
}

// ============================================================================
// Settings widget — inherits SharedOutputSettingsWidget (gives us the
// shared path / width / height / rate fields)
// ============================================================================

class PipewireOutputSettingsWidget final : public Gfx::SharedOutputSettingsWidget
{
public:
  PipewireOutputSettingsWidget(QWidget* parent = nullptr)
      : Gfx::SharedOutputSettingsWidget{parent}
  {
    // Pixel-format combo for HDR / wide-gamut pipelines + a DMA-BUF
    // toggle for Vulkan zero-copy output. Both choices ride on the
    // path's `?key=value` query since SharedOutputSettings has no
    // native fields for them.
    m_formatEdit = new QComboBox(this);
    m_formatEdit->addItems(
        {"rgba8", "bgra8", "rgb10a2", "bgr10a2", "rgba16f", "rgba32f"});
    m_layout->addRow(tr("Pixel Format"), m_formatEdit);

    m_dmabufEdit = new QCheckBox(tr("Zero-copy DMA-BUF"), this);
    m_dmabufEdit->setToolTip(
        tr("Allocate exportable images and publish them as DMA-BUF "
           "buffers to pipewire. The backend is selected automatically "
           "from the live QRhi:\n"
           " - Vulkan: VkImage with VK_EXT_image_drm_format_modifier + "
           "VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT.\n"
           " - OpenGL (EGL): GBM buffer object + EGL DMA-BUF import.\n"
           "Falls back to CPU readback when neither path is usable."));
    m_layout->addRow(QString(), m_dmabufEdit);

    setSettings(OutputFactory{}.defaultSettings());
  }

  Device::DeviceSettings getSettings() const override
  {
    auto s = SharedOutputSettingsWidget::getSettings();
    s.protocol = OutputFactory::static_concreteKey();

    auto set = s.deviceSpecificSettings.value<SharedOutputSettings>();
    QString path = set.path;
    const int q = path.indexOf('?');
    if(q >= 0)
      path.truncate(q);
    QString query = "format=" + m_formatEdit->currentText();
    if(m_dmabufEdit->isChecked())
      query += "&dmabuf=on";
    set.path = path + "?" + query;
    s.deviceSpecificSettings = QVariant::fromValue(set);
    return s;
  }

  void setSettings(const Device::DeviceSettings& settings) override
  {
    Gfx::SharedOutputSettingsWidget::setSettings(settings);
    const auto set
        = settings.deviceSpecificSettings.value<SharedOutputSettings>();
    static const QRegularExpression formatRe("format=([^&]+)");
    auto m = formatRe.match(set.path);
    if(m.hasMatch())
    {
      const int idx = m_formatEdit->findText(m.captured(1));
      if(idx >= 0)
        m_formatEdit->setCurrentIndex(idx);
    }
    static const QRegularExpression dmabufRe("dmabuf=(on|true|1)");
    m_dmabufEdit->setChecked(dmabufRe.match(set.path).hasMatch());
  }

private:
  QComboBox* m_formatEdit{};
  QCheckBox* m_dmabufEdit{};
};

// ============================================================================
// OutputFactory
// ============================================================================

QString OutputFactory::prettyName() const noexcept
{
  return QObject::tr("PipeWire Video Output");
}

QUrl OutputFactory::manual() const noexcept
{
  return QUrl{
      "https://ossia.io/score-docs/devices/output-devices.html#pipewire-output"};
}

Device::DeviceInterface* OutputFactory::makeDevice(
    const Device::DeviceSettings& settings,
    const Explorer::DeviceDocumentPlugin& /*plugin*/,
    const score::DocumentContext& ctx)
{
  return new PipewireOutputDevice(settings, ctx);
}

const Device::DeviceSettings& OutputFactory::defaultSettings() const noexcept
{
  static const Device::DeviceSettings s = [] {
    Device::DeviceSettings d;
    d.name = "PipeWire Output";
    d.protocol = OutputFactory::static_concreteKey();
    SharedOutputSettings set;
    set.path = "score-output?format=rgba8";
    set.width = 1280;
    set.height = 720;
    set.rate = 30.;
    d.deviceSpecificSettings = QVariant::fromValue(set);
    return d;
  }();
  return s;
}

Device::ProtocolSettingsWidget* OutputFactory::makeSettingsWidget()
{
  return new PipewireOutputSettingsWidget;
}

score::gfx::OutputNode* makePipewireOutput(const Gfx::SharedOutputSettings& s)
{
  return new PipewireOutputNode{s};
}

int pipewireOutputFramesQueued(const score::gfx::OutputNode& node)
{
  if(auto* n = dynamic_cast<const PipewireOutputNode*>(&node); n && n->m_producer)
    return n->m_producer->m_framesQueued.load(std::memory_order_relaxed);
  return 0;
}

bool pipewireOutputDmabufEngaged(const score::gfx::OutputNode& node)
{
  if(auto* n = dynamic_cast<const PipewireOutputNode*>(&node))
  {
    bool engaged = false;
#if defined(SCORE_PIPEWIRE_OUT_DMABUF)
    engaged = engaged || n->m_dmabufMode;
#endif
#if defined(SCORE_PIPEWIRE_OUT_DMABUF_EGL)
    engaged = engaged || n->m_dmabufEglMode;
#endif
    return engaged;
  }
  return false;
}

} // namespace Gfx::PipeWire

W_OBJECT_IMPL(Gfx::PipeWire::PipewireOutputDevice)
