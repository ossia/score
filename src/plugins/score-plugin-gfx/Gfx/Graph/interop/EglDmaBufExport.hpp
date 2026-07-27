#pragma once

// SPDX-License-Identifier: GPL-3.0-or-later
//
// EGL DMA-BUF exporter, sister to EglDmaBufImporter, for the PipeWire video
// output path when the live QRhi backend is OpenGLES2: allocate a GBM buffer
// object on /dev/dri/renderD128, export its DMA-BUF FD plus stride, offset and
// modifier, hand the FD to PipeWire as a SPA_DATA_DmaBuf, and import the same
// buffer back into the local EGL display as an EGLImage so the renderer can
// target it through a persistent GL texture id and a QRhi createFrom wrapper.
//
// GBM rather than EGL_MESA_image_dma_buf_export because
// SPA_FORMAT_VIDEO_modifier must be advertised before the first buffer is
// allocated, and only GBM offers the modifier list up front. It is also a flat
// C ABI, dlopen-friendly, and ships with every Mesa install.
//
// Buffers are created USE_RENDERING | USE_LINEAR: GPU-renderable on the
// producer side, LINEAR-tiled so any consumer can sample them.
//
// Sync: glFinish() on the producer is sufficient before pw_stream_queue_buffer,
// since a consumer importing the FD gets its own kernel-side barrier. Tighter
// control would mean EGL_KHR_fence_sync -> EGL_ANDROID_native_fence_sync
// stamped into explicit-sync metadata, the same upgrade path as the Vulkan
// output.

#if defined(__linux__)
#include <Gfx/Graph/interop/EglDmaBufImport.hpp>

#include <ossia/detail/dylib_loader.hpp>

#include <QDebug>
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>

#include <fcntl.h>
#include <unistd.h>

#include <cstdint>
#include <cstring>
#include <optional>

namespace score::gfx
{

/** Minimal GBM ABI surface. We don't include `<gbm.h>` directly so the
 *  build doesn't require libgbm-dev present — dlopen + runtime detect. */
struct GbmDmaBufExport
{
  struct gbm_device;
  struct gbm_bo;

  // GBM_BO_USE_SCANOUT — required for a buffer the display engine reads
  static constexpr uint32_t GBM_BO_USE_SCANOUT_v = 1u << 0;
  // GBM_BO_USE_RENDERING | GBM_BO_USE_LINEAR — see /usr/include/gbm.h
  static constexpr uint32_t GBM_BO_USE_RENDERING_v = (1u << 2);
  static constexpr uint32_t GBM_BO_USE_LINEAR_v = (1u << 4);

  // DRM fourccs from <drm_fourcc.h> — fourcc_code('A','R','2','4')
  // expands to the 32-bit little-endian char concatenation.
  static constexpr uint32_t DRM_FORMAT_ARGB8888_v = 0x34325241u; // "AR24"
  static constexpr uint32_t DRM_FORMAT_ABGR8888_v = 0x34324241u; // "AB24"
  static constexpr uint32_t DRM_FORMAT_XRGB8888_v = 0x34325258u; // "XR24"
  static constexpr uint64_t DRM_FORMAT_MOD_LINEAR_v = 0ULL;

  using FN_gbm_create_device = gbm_device* (*)(int fd);
  using FN_gbm_device_destroy = void (*)(gbm_device* gbm);
  using FN_gbm_bo_create
      = gbm_bo* (*)(gbm_device*, uint32_t w, uint32_t h, uint32_t fmt,
                    uint32_t flags);
  using FN_gbm_bo_create_with_modifiers2
      = gbm_bo* (*)(gbm_device*, uint32_t w, uint32_t h, uint32_t fmt,
                    const uint64_t* mods, unsigned int count, uint32_t flags);
  using FN_gbm_bo_get_fd = int (*)(gbm_bo*);
  using FN_gbm_bo_get_stride = uint32_t (*)(gbm_bo*);
  using FN_gbm_bo_get_offset = uint32_t (*)(gbm_bo*, int plane);
  using FN_gbm_bo_get_modifier = uint64_t (*)(gbm_bo*);
  using FN_gbm_bo_get_plane_count = int (*)(gbm_bo*);
  using FN_gbm_bo_destroy = void (*)(gbm_bo*);

  std::optional<ossia::dylib_loader> m_lib;
  FN_gbm_create_device m_create_device{};
  FN_gbm_device_destroy m_device_destroy{};
  FN_gbm_bo_create m_bo_create{};
  FN_gbm_bo_create_with_modifiers2 m_bo_create_with_modifiers2{};
  FN_gbm_bo_get_fd m_bo_get_fd{};
  FN_gbm_bo_get_stride m_bo_get_stride{};
  FN_gbm_bo_get_offset m_bo_get_offset{};
  FN_gbm_bo_get_modifier m_bo_get_modifier{};
  FN_gbm_bo_get_plane_count m_bo_get_plane_count{};
  FN_gbm_bo_destroy m_bo_destroy{};

  int m_drmFd{-1};
  gbm_device* m_device{nullptr};

  /** Quick capability probe: is GBM + render-node access workable?
   *  Cheaper than full init() — used to decide whether to enable the
   *  EGL DMA-BUF output path before touching pipewire. */
  static bool isAvailable() noexcept
  {
    try
    {
      ossia::dylib_loader probe(
          std::vector<std::string_view>{"libgbm.so.1", "libgbm.so"});
      if(!probe.symbol<FN_gbm_create_device>("gbm_create_device"))
        return false;
    }
    catch(...)
    {
      return false;
    }
    // Render node must exist + be openable. ::access(F_OK) is the
    // cheapest check — full open happens in init().
    return ::access("/dev/dri/renderD128", R_OK | W_OK) == 0;
  }

  bool init() noexcept
  {
    try
    {
      m_lib.emplace(
          std::vector<std::string_view>{"libgbm.so.1", "libgbm.so"});
    }
    catch(...)
    {
      return false;
    }

    m_create_device
        = m_lib->symbol<FN_gbm_create_device>("gbm_create_device");
    m_device_destroy
        = m_lib->symbol<FN_gbm_device_destroy>("gbm_device_destroy");
    m_bo_create = m_lib->symbol<FN_gbm_bo_create>("gbm_bo_create");
    // _with_modifiers2 is newer (Mesa 22+); fall back to plain
    // _with_modifiers (no flags arg) or finally _bo_create.
    m_bo_create_with_modifiers2
        = m_lib->symbol<FN_gbm_bo_create_with_modifiers2>(
            "gbm_bo_create_with_modifiers2");
    m_bo_get_fd = m_lib->symbol<FN_gbm_bo_get_fd>("gbm_bo_get_fd");
    m_bo_get_stride
        = m_lib->symbol<FN_gbm_bo_get_stride>("gbm_bo_get_stride");
    m_bo_get_offset
        = m_lib->symbol<FN_gbm_bo_get_offset>("gbm_bo_get_offset");
    m_bo_get_modifier
        = m_lib->symbol<FN_gbm_bo_get_modifier>("gbm_bo_get_modifier");
    m_bo_get_plane_count = m_lib->symbol<FN_gbm_bo_get_plane_count>(
        "gbm_bo_get_plane_count");
    m_bo_destroy = m_lib->symbol<FN_gbm_bo_destroy>("gbm_bo_destroy");

    if(!m_create_device || !m_device_destroy || !m_bo_create
       || !m_bo_get_fd || !m_bo_get_stride || !m_bo_destroy)
      return false;

    // Open the DRM render node. The render node is the privilege-free
    // entry point — it lets unprivileged processes do GPU rendering
    // without needing master access to the display. /dev/dri/renderD128
    // is the first card; for multi-GPU systems an explicit selection
    // hook would go through eglQueryDevicesEXT, but it's not needed
    // for the single-GPU 99% case.
    m_drmFd = ::open("/dev/dri/renderD128", O_RDWR | O_CLOEXEC);
    if(m_drmFd < 0)
    {
      qWarning() << "EglDmaBufExport: failed to open /dev/dri/renderD128";
      return false;
    }

    m_device = m_create_device(m_drmFd);
    if(!m_device)
    {
      qWarning() << "EglDmaBufExport: gbm_create_device failed";
      ::close(m_drmFd);
      m_drmFd = -1;
      return false;
    }
    return true;
  }

  void shutdown() noexcept
  {
    if(m_device && m_device_destroy)
      m_device_destroy(m_device);
    m_device = nullptr;
    if(m_drmFd >= 0)
      ::close(m_drmFd);
    m_drmFd = -1;
    m_lib.reset();
  }

  /** One allocated DMA-BUF slot. Owns the gbm_bo + exported FD; the
   *  imported EGLImage + GL texture id live on the GL side and get
   *  cleaned up by destroySlot(). */
  struct Slot
  {
    gbm_bo* bo{nullptr};
    int fd{-1};
    uint32_t stride{};
    uint32_t offset{};
    uint64_t modifier{};
    uint32_t width{};
    uint32_t height{};
    uint32_t drm_fourcc{};
    EglDmaBufImporter::PlaneImport plane{}; // EGLImage
    unsigned int glTexture{0};
    std::size_t size{};
  };

  /** Step 1 of slot allocation: GBM-only, thread-safe (no GL context
   *  required). Use this when the EGL import + GL texture creation
   *  must happen on a different thread than the pipewire callback that
   *  asked us to allocate. */
  bool allocSlotGbmOnly(
      Slot& out, uint32_t w, uint32_t h, uint32_t drm_fourcc,
      uint32_t extraFlags = 0) noexcept
  {
    out = {};
    out.width = w;
    out.height = h;
    out.drm_fourcc = drm_fourcc;

    // Try the modifier-aware allocator first. Pass exactly one modifier
    // (LINEAR) so GBM has no choice but to give us LINEAR — symmetric
    // with what the SPA pod advertises in PipewireOutputDevice.cpp.
    const uint64_t mods[] = {DRM_FORMAT_MOD_LINEAR_v};
    const uint32_t flags
        = GBM_BO_USE_RENDERING_v | GBM_BO_USE_LINEAR_v | extraFlags;
    if(m_bo_create_with_modifiers2)
    {
      out.bo = m_bo_create_with_modifiers2(
          m_device, w, h, drm_fourcc, mods,
          sizeof(mods) / sizeof(mods[0]), flags);
    }
    if(!out.bo)
    {
      // Fallback: plain gbm_bo_create with USE_RENDERING | USE_LINEAR;
      // the driver picks the modifier (will be LINEAR with USE_LINEAR).
      out.bo = m_bo_create(m_device, w, h, drm_fourcc, flags);
    }
    if(!out.bo)
    {
      // NVIDIA's GBM backend (nvidia-drm) rejects USE_RENDERING and the
      // with_modifiers2 entry point outright, but allocates fine with
      // USE_LINEAR alone (verified: driver 595, all 8888 fourccs). The
      // BO is only ever written through an EGLImage-bound GL texture
      // copy, so the RENDERING usage bit is not load-bearing for us.
      out.bo = m_bo_create(
          m_device, w, h, drm_fourcc, GBM_BO_USE_LINEAR_v | extraFlags);
    }
    if(!out.bo)
    {
      qWarning() << "EglDmaBufExport: gbm_bo_create failed";
      return false;
    }

    out.fd = m_bo_get_fd(out.bo);
    out.stride = m_bo_get_stride(out.bo);
    out.offset = m_bo_get_offset ? m_bo_get_offset(out.bo, 0) : 0;
    out.modifier
        = m_bo_get_modifier ? m_bo_get_modifier(out.bo) : 0ULL;
    out.size = std::size_t(out.stride) * std::size_t(h);

    if(out.fd < 0)
    {
      qWarning() << "EglDmaBufExport: gbm_bo_get_fd returned -1";
      if(m_bo_destroy)
        m_bo_destroy(out.bo);
      out.bo = nullptr;
      return false;
    }
    return true;
  }

  /** Allocate one slot end-to-end (single-thread usage):
   *    1. gbm_bo with USE_RENDERING | USE_LINEAR
   *    2. export DMA-BUF FD
   *    3. import back into EGL via importer
   *    4. createFrom-ready GL texture id stored in slot.glTexture
   *
   *  On any failure, partially-acquired resources are released before
   *  returning false. Requires a current GL context. */
  bool allocSlot(
      Slot& out, uint32_t w, uint32_t h, uint32_t drm_fourcc,
      EglDmaBufImporter& importer) noexcept
  {
    if(!allocSlotGbmOnly(out, w, h, drm_fourcc))
      return false;

    // Create a fresh GL texture to receive the import. Persistent: the
    // OutputNode createFrom-wraps it once and the same texture id
    // serves every frame this slot is the active pipewire buffer.
    out.glTexture = createLinearClampGlTexture2D();
    if(out.glTexture == 0)
    {
      qWarning() << "EglDmaBufExport: failed to create GL texture";
      ::close(out.fd);
      out.fd = -1;
      if(m_bo_destroy)
        m_bo_destroy(out.bo);
      out.bo = nullptr;
      return false;
    }

    // Import the same DMA-BUF back into our local EGL display and
    // bind it to glTexture so the renderer sees an image-backed texture.
    if(!importer.importPlane(
           out.plane, out.glTexture, out.fd, out.modifier, out.offset,
           out.stride, drm_fourcc, int(w), int(h)))
    {
      qWarning() << "EglDmaBufExport: importer.importPlane failed";
      if(auto* ctx = QOpenGLContext::currentContext(); ctx)
        ctx->extraFunctions()->glDeleteTextures(1, &out.glTexture);
      out.glTexture = 0;
      ::close(out.fd);
      out.fd = -1;
      if(m_bo_destroy)
        m_bo_destroy(out.bo);
      out.bo = nullptr;
      return false;
    }
    return true;
  }

  /** Counterpart to allocSlotGbmOnly: releases the FD and the BO without
   *  requiring an EGL importer or a current GL context. For consumers that
   *  never imported the buffer locally (V4L2 hands it to the kernel). */
  void destroySlotGbmOnly(Slot& s) noexcept
  {
    if(s.fd >= 0)
    {
      ::close(s.fd);
      s.fd = -1;
    }
    if(s.bo && m_bo_destroy)
      m_bo_destroy(s.bo);
    s.bo = nullptr;
  }

  void destroySlot(Slot& s, EglDmaBufImporter& importer) noexcept
  {
    if(s.plane.image)
      importer.cleanupPlane(s.plane);
    if(s.glTexture)
    {
      if(auto* ctx = QOpenGLContext::currentContext(); ctx)
        ctx->extraFunctions()->glDeleteTextures(1, &s.glTexture);
      s.glTexture = 0;
    }
    // Producer owns the FD (we got it via gbm_bo_get_fd, which dups);
    // pipewire just borrows it for the buffer's lifetime. Mirrors the
    // Vulkan output path's on_remove_buffer FD handling.
    if(s.fd >= 0)
    {
      ::close(s.fd);
      s.fd = -1;
    }
    if(s.bo && m_bo_destroy)
      m_bo_destroy(s.bo);
    s.bo = nullptr;
  }
};

} // namespace score::gfx
#endif // __linux__
