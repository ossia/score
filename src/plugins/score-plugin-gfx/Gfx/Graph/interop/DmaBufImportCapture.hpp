#pragma once

/**
 * @file DmaBufImportCapture.hpp
 * @brief Zero-copy capture strategy: import a producer's DMA-BUF ring straight
 *        into the renderer's sampled texture.
 *
 * The capture-side twin of DRMPrime.hpp, for producers that hand out a *fixed*
 * set of DMA-BUF file descriptors for the lifetime of a stream (V4L2
 * MMAP+EXPBUF or V4L2_MEMORY_DMABUF, and anything else shaped like it) rather
 * than one fd per frame. Because the fds are fixed, every import happens once
 * at init(); the per-frame cost is a single re-bind of the already-imported
 * image, and no pixel is ever copied.
 *
 * Backend dispatch, same two rungs as DRMPrimeDecoder:
 *   - QRhi::Vulkan    -> DMABufPlaneImporter (VK_KHR_external_memory_fd +
 *                        VK_EXT_image_drm_format_modifier), one VkImage per
 *                        slot, re-pointed into the QRhiTexture per frame.
 *   - QRhi::OpenGLES2 -> EglDmaBufImporter (EGL_EXT_image_dma_buf_import_
 *                        modifiers + GL_OES_EGL_image), one EGLImage per slot,
 *                        re-targeted onto one persistent GL texture per frame.
 *   - anything else, or an import the driver refuses -> init() returns false so
 *                        the renderer degrades to the CPU-staging rung.
 *
 * Geometry and format come from the caller's `outputTexture`: the wire decoder
 * already sized it to the producer's byte layout (a UYVY 4:2:2 frame is an
 * RGBA8 texture of half the width), so importing at exactly that size and
 * format reinterprets the same bytes the CPU rung would have uploaded.
 *
 * Planar layouts (NV12 and friends) import one texture per plane. Where each
 * plane starts comes from one of two places:
 *
 *   - the producer, when it states a layout in DmaBufSlotDesc::planes. Used
 *     verbatim. Real allocators do things a derivation cannot express --
 *     NvBufSurface aligns every plane offset to 64 KB even with row padding
 *     disabled -- so a producer that knows should say.
 *   - otherwise derived from the decoder's own plane textures, exactly as
 *     CpuStagedCapture derives it: each plane's byte size is its texture
 *     geometry times its texel size, offsets accumulating in order. That keeps
 *     the two rungs agreeing about where chroma begins for a V4L2-style
 *     producer, and a new planar decoder needs no change here.
 *
 * The derivation assumes tightly-packed rows, so a producer that neither states
 * its layout nor packs tightly is refused rather than rendered wrong.
 *
 * BORROWED-BUFFER LIFETIME (the contract the producer must honour)
 * ---------------------------------------------------------------
 * The renderer samples producer memory directly, so a slot must not go back to
 * the producer's device while the GPU may still read it. Ownership of a slot is
 * held by exactly one party at a time:
 *
 *   producer thread  ingestFrame(i)   -- slot i becomes render-owned. If a
 *                                        previously published slot had not been
 *                                        consumed yet, it is handed straight
 *                                        back (it was never bound).
 *   render thread    acquireForRender -- takes the published slot, binds it,
 *                                        and retires the slot it displaced.
 *   render thread                     -- a retired slot only becomes returnable
 *                                        after `FramesInFlight + 1` further
 *                                        acquisitions, which is the point at
 *                                        which QRhi guarantees the frame that
 *                                        last sampled it has completed on the
 *                                        GPU. (Acquisitions are counted, not
 *                                        QRhi frames, and there is at most one
 *                                        acquisition per QRhi frame, so the
 *                                        count is conservative.)
 *   producer thread  takeReturnedSlots -- drains the bitmask of slots that are
 *                                        safe to give back to the device.
 *
 * A producer that never calls takeReturnedSlots() starves its own queue; one
 * that returns a slot without it corrupts the frame being displayed.
 *
 * The imported images are created with a DRM format modifier and EXCLUSIVE
 * sharing, and no queue-family-foreign acquire barrier is issued -- same as
 * DRMPrimeDecoder. That is sound for linear modifiers, which is all a V4L2
 * exporter produces; a tiled vendor modifier would need the barrier added.
 */

#if defined(__linux__)
#include <Gfx/Graph/interop/CaptureStrategyCommon.hpp>
#include <Gfx/Graph/interop/DMABufImport.hpp>
#include <Gfx/Graph/interop/DrmFourcc.hpp>
#include <Gfx/Graph/interop/EglDmaBufImport.hpp>
#include <Gfx/Graph/interop/VideoCaptureStrategy.hpp>

#include <QtGui/private/qrhi_p.h>

#include <QDebug>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

/// DMABufPlaneImporter only exists when the Vulkan headers carry both external-
/// memory extensions AND Qt is new enough to expose the native device handles
/// it needs (see DMABufImport.hpp's own guard). Older Qt keeps the EGL rung.
#if QT_HAS_VULKAN && defined(VK_EXT_image_drm_format_modifier)   \
    && defined(VK_KHR_external_memory_fd)                        \
    && QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
#define SCORE_GFX_HAS_VK_DMABUF_IMPORT 1
#endif

namespace score::gfx::interop
{

/// Where one plane starts inside a producer buffer.
struct DmaBufPlaneLayout
{
  std::uint32_t offset{};
  std::uint32_t pitch{};
};

/// One producer buffer, fixed for the lifetime of the stream.
struct DmaBufSlotDesc
{
  int fd{-1};
  std::uint64_t modifier{}; ///< DRM_FORMAT_MOD_*; 0 == LINEAR
  std::uint32_t offset{};
  std::uint32_t pitch{}; ///< bytes per row, as the producer lays them out

  /// Explicit plane layout, for producers that know it.
  ///
  /// `planeCount == 0` means "derive it", which is right for a V4L2-style
  /// producer that hands out one tightly-packed buffer and nothing else. But a
  /// derivation cannot express what a real allocator does: NvBufSurface aligns
  /// each plane's *offset* to 64 KB even when row padding is disabled, so a
  /// derived chroma offset points at the wrong address and the rung has to
  /// decline. Producers that can state the layout should, and then it is used
  /// verbatim.
  std::uint32_t planeCount{0};
  DmaBufPlaneLayout planes[3]{};
};

/// QRhiTexture format -> the native format tokens the two importers need.
/// `bytesPerPixel` is what the pitch is validated against.
struct DmaBufImportFormat
{
  bool ok{};
  std::uint32_t drmFourcc{};
  std::uint32_t bytesPerPixel{};
#if defined(SCORE_GFX_HAS_VK_DMABUF_IMPORT)
  VkFormat vk{VK_FORMAT_UNDEFINED};
#endif
};

inline DmaBufImportFormat dmaBufFormatFor(QRhiTexture::Format f) noexcept
{
  // Mesa-defined single/dual-channel fourccs, spelled out so score builds
  // without <drm/drm_fourcc.h> (same convention as DrmFourcc.hpp).
  constexpr std::uint32_t DRM_R8 = 0x20203852u;     // 'R8  '
  constexpr std::uint32_t DRM_GR88 = 0x38385247u;   // 'GR88'
  constexpr std::uint32_t DRM_R16 = 0x20363152u;    // 'R16 '
  constexpr std::uint32_t DRM_GR1616 = 0x36315247u; // 'GR16'

  DmaBufImportFormat r;
  auto set = [&](std::uint32_t fourcc, std::uint32_t bpp
#if defined(SCORE_GFX_HAS_VK_DMABUF_IMPORT)
                 ,
                 VkFormat vk
#endif
             ) {
    r.ok = true;
    r.drmFourcc = fourcc;
    r.bytesPerPixel = bpp;
#if defined(SCORE_GFX_HAS_VK_DMABUF_IMPORT)
    r.vk = vk;
#endif
  };

#if defined(SCORE_GFX_HAS_VK_DMABUF_IMPORT)
#define SCORE_DMABUF_FMT(fourcc, bpp, vk) set(fourcc, bpp, vk)
#else
#define SCORE_DMABUF_FMT(fourcc, bpp, vk) set(fourcc, bpp)
#endif

  switch(f)
  {
    case QRhiTexture::RGBA8:
      SCORE_DMABUF_FMT(DRM_ABGR8888, 4, VK_FORMAT_R8G8B8A8_UNORM);
      break;
    case QRhiTexture::BGRA8:
      SCORE_DMABUF_FMT(DRM_ARGB8888, 4, VK_FORMAT_B8G8R8A8_UNORM);
      break;
    case QRhiTexture::R8:
      SCORE_DMABUF_FMT(DRM_R8, 1, VK_FORMAT_R8_UNORM);
      break;
    case QRhiTexture::RG8:
      SCORE_DMABUF_FMT(DRM_GR88, 2, VK_FORMAT_R8G8_UNORM);
      break;
    case QRhiTexture::R16:
      SCORE_DMABUF_FMT(DRM_R16, 2, VK_FORMAT_R16_UNORM);
      break;
    case QRhiTexture::RG16:
      SCORE_DMABUF_FMT(DRM_GR1616, 4, VK_FORMAT_R16G16_UNORM);
      break;
    case QRhiTexture::RGB10A2:
      SCORE_DMABUF_FMT(
          DRM_ABGR2101010, 4, VK_FORMAT_A2B10G10R10_UNORM_PACK32);
      break;
    default:
      break;
  }
#undef SCORE_DMABUF_FMT
  return r;
}

struct DmaBufImportCapture final : VideoCaptureStrategy
{
  /// Slot indices are carried to the producer thread in a 32-bit mask.
  static constexpr std::size_t kMaxSlots = 32;

  /// @param tag  vendor prefix for name(), e.g. "V4L2".
  /// @param slots  the producer's fixed fd ring; must outlive nothing (the fds
  ///               are dup()'d by the Vulkan importer and referenced by the EGL
  ///               importer only during init).
  DmaBufImportCapture(const char* tag, std::vector<DmaBufSlotDesc> slots)
      : m_slots{std::move(slots)}
      , m_tag{tag}
      , m_name{m_tag + "-dmabuf"}
  {
  }

  ~DmaBufImportCapture() override { release(); }

  /// Ask for the whole frame to be imported as one external image rather than
  /// per-plane 2D images. @p fourcc is the frame's DRM fourcc (DRM_FORMAT_NV12
  /// and friends) and @p height its full height.
  ///
  /// The caller must pair this with a decoder that samples an external texture
  /// and does no colour conversion of its own -- the driver's sampler already
  /// did it. Set it before init().
  void requestExternalImage(std::uint32_t fourcc, int height) noexcept
  {
    m_wholeFrameFourcc = fourcc;
    m_wholeFrameHeight = height;
  }

  /// True once init() settled on the external form, so the caller can report
  /// which shape actually engaged rather than which one it asked for.
  bool usesExternalImage() const noexcept { return m_external; }

  const char* name() const noexcept override { return m_name.c_str(); }

  std::size_t slotCount() const noexcept override { return m_slots.size(); }

  /// No host pointer exists on this rung: the producer's device writes the
  /// buffer directly and the GPU samples it in place.
  void* slotBuffer(std::size_t) const noexcept override { return nullptr; }

  QRhiTexture* outputTexture() const noexcept override { return m_tex; }

  std::size_t outputPlaneCount() const noexcept override
  {
    return m_planeInfo.empty() ? 1 : m_planeInfo.size();
  }

  QRhiTexture* outputPlane(std::size_t i) const noexcept override
  {
    return i < m_planeInfo.size() ? m_planeTex[i] : nullptr;
  }

  bool init(const VideoCaptureStrategyConfig& c) override
  {
    cfg = c;
    if(!cfg.rhi || !cfg.outputTexture)
      return false;
    if(m_slots.empty() || m_slots.size() > kMaxSlots)
      return false;

    // Plane geometry. A planar frame is one contiguous dma-buf holding N planes
    // back to back, and the decoder's own texture list defines the split: each
    // plane's byte size is its texture geometry times its texel size, and
    // offsets accumulate in order. That is the same derivation CpuStagedCapture
    // uses, so the two rungs cannot disagree about where chroma starts, and
    // adding a planar decoder needs no change here.
    m_planeInfo.clear();
    // A producer that states its plane layout is believed; only one that does
    // not gets the derivation.
    const bool explicitLayout
        = !m_slots.empty() && m_slots[0].planeCount >= cfg.planes.size();
    if(cfg.planes.size() > 1)
    {
      std::uint32_t off = 0;
      for(std::size_t i = 0; i < cfg.planes.size(); ++i)
      {
        auto* tex = cfg.planes[i];
        if(!tex)
        {
          qDebug() << "DmaBufImportCapture: decoder plane texture is null";
          return false;
        }
        const auto psz = tex->pixelSize();
        const auto pf = dmaBufFormatFor(tex->format());
        if(!pf.ok)
        {
          qDebug() << "DmaBufImportCapture: no DMA-BUF mapping for plane format"
                   << int(tex->format());
          return false;
        }
        const auto derivedPitch = std::uint32_t(psz.width()) * pf.bytesPerPixel;
        const auto pitch
            = explicitLayout ? m_slots[0].planes[i].pitch : derivedPitch;
        const auto planeOff = explicitLayout ? m_slots[0].planes[i].offset : off;
        if(pitch < derivedPitch)
        {
          qDebug() << "DmaBufImportCapture: plane" << i << "pitch" << pitch
                   << "cannot hold" << psz.width() << "px of"
                   << pf.bytesPerPixel << "bytes";
          return false;
        }
        m_planeInfo.push_back(
            PlaneInfo{tex->format(), pf, planeOff, pitch, psz.width(),
                      psz.height()});
        off = planeOff + pitch * std::uint32_t(psz.height());
      }
      m_planeBytes = off;
    }
    else
    {
      const auto pf = dmaBufFormatFor(cfg.outputTexture->format());
      if(!pf.ok)
      {
        qDebug() << "DmaBufImportCapture: no DMA-BUF mapping for QRhi format"
                 << int(cfg.outputTexture->format());
        return false;
      }
      const auto sz0 = cfg.outputTexture->pixelSize();
      m_planeInfo.push_back(
          PlaneInfo{
              cfg.outputTexture->format(), pf, 0,
              std::uint32_t(sz0.width()) * pf.bytesPerPixel, sz0.width(),
              sz0.height()});
      m_planeBytes = 0;
    }

    // Plane 0's geometry still drives the slot-pitch validation below, and for
    // the single-plane case it is the whole picture.
    const auto sz = QSize{m_planeInfo[0].width, m_planeInfo[0].height};
    const auto qfmt = m_planeInfo[0].qfmt;
    const auto fmt = m_planeInfo[0].fmt;

    // The DERIVATION assumes tightly-packed rows; a producer that stated its
    // layout has already told us where each plane is and is exempt.
    if(m_planeInfo.size() > 1 && !explicitLayout)
    {
      const auto tight = std::uint32_t(sz.width()) * fmt.bytesPerPixel;
      for(const auto& s : m_slots)
      {
        if(s.pitch != tight)
        {
          qDebug() << "DmaBufImportCapture: planar source has a padded pitch"
                   << s.pitch << "(tight" << tight
                   << "); plane offsets cannot be derived, no dma-buf rung";
          return false;
        }
      }
    }
    if(sz.width() <= 0 || sz.height() <= 0)
      return false;

    // For the external form the decoder's texture format is nominal -- the
    // external image carries its own -- so validating the producer's pitch
    // against it compares an RGBA8 row against an NV12 luma row and rejects a
    // perfectly good buffer. There, plane 0's stated pitch is the truth.
    const bool wantsExternal = m_wholeFrameFourcc != 0;
    const auto minPitch
        = wantsExternal ? std::uint32_t(sz.width())
                        : static_cast<std::uint32_t>(sz.width()) * fmt.bytesPerPixel;
    for(const auto& s : m_slots)
    {
      if(s.fd < 0 || s.pitch < minPitch)
      {
        qDebug() << "DmaBufImportCapture: slot fd" << s.fd << "pitch" << s.pitch
                 << "cannot hold" << sz.width() << "x" << sz.height()
                 << "(need >=" << minPitch << ")";
        return false;
      }
    }

    m_retireDepth = static_cast<std::size_t>(
                        cfg.rhi->resourceLimit(QRhi::FramesInFlight))
                    + 1u;
    if(m_retireDepth + 2u > m_slots.size())
    {
      qDebug() << "DmaBufImportCapture: producer ring of" << m_slots.size()
               << "is too shallow for" << m_retireDepth
               << "frames in flight plus the device's own queue";
      return false;
    }

#if defined(SCORE_GFX_HAS_VK_DMABUF_IMPORT)
    if(DMABufPlaneImporter::isAvailable(*cfg.rhi))
    {
      // The NVIDIA proprietary driver imports a V4L2 dma_buf and samples
      // plausible pixels from it, but the image does not stay bound to the
      // buffer it was imported from: measured against a 30fps uvcvideo source
      // on an RTX 3060, the sampled content changed ~93 times a second, three
      // times the rate at which frames were acquired, while the identical code
      // on anv and on Mesa's GL path changed exactly once per frame. There is
      // no capability bit for that, and a rung that tears is worse than one
      // that degrades.
#if defined(VK_DRIVER_ID_NVIDIA_PROPRIETARY)
      constexpr auto kNvidiaProprietary
          = std::uint32_t(VK_DRIVER_ID_NVIDIA_PROPRIETARY);
#else
      constexpr std::uint32_t kNvidiaProprietary = 4;
#endif
      if(DMABufPlaneImporter::driverId(*cfg.rhi) == kNvidiaProprietary)
      {
        qDebug() << "DmaBufImportCapture: NVIDIA proprietary Vulkan does not "
                    "hold an imported DMA-BUF still; no Vulkan rung";
        return false;
      }
      // Set before importing: release() keys its cleanup off the backend, and
      // a refusal halfway through the ring still has images to destroy.
      m_backend = Backend::Vulkan;
      m_vk.init(*cfg.rhi);
      for(std::size_t i = 0; i < m_slots.size(); ++i)
      {
        const auto& s = m_slots[i];
        for(std::size_t p = 0; p < m_planeInfo.size(); ++p)
        {
          const auto& pi = m_planeInfo[p];
          if(!m_vk.importPlane(
                 m_vkSlots[i][p], s.fd, s.modifier, s.offset + pi.offset,
                 pi.pitch, pi.fmt.vk, pi.width, pi.height))
          {
            qDebug() << "DmaBufImportCapture: Vulkan import refused slot" << i
                     << "plane" << p << "fd" << s.fd << "modifier" << Qt::hex
                     << s.modifier;
            release();
            return false;
          }
        }
      }
      m_name += "/Vulkan";
    }
    else
#endif
        if(EglDmaBufImporter::isAvailable(*cfg.rhi) && m_egl.init(*cfg.rhi))
    {
      m_backend = Backend::OpenGL;
      // Refusing here is the whole point of the check: eglCreateImage accepts a
      // modifier the driver only exposes through GL_TEXTURE_EXTERNAL_OES, and
      // the resulting GL_TEXTURE_2D samples black without raising an error.
      // A planar frame goes in as ONE external image when the caller asked for
      // it: drivers refuse the per-plane 2D form (fourcc 'R8  ' is rejected on
      // both Mesa and Tegra), and an external sampler converts YUV itself.
      m_external = cfg.planes.size() == 1 && m_wholeFrameFourcc != 0
                   && m_egl.hasExternalImage();

      if(!m_external)
      {
        // Per plane: NV12's chroma is a different fourcc from its luma, and a
        // driver may expose one as a 2D texture and not the other.
        for(const auto& pi : m_planeInfo)
        {
          if(!m_egl.canImportModifier(pi.fmt.drmFourcc, m_slots[0].modifier))
          {
            qDebug() << "DmaBufImportCapture: driver cannot sample fourcc"
                     << Qt::hex << pi.fmt.drmFourcc << "modifier"
                     << m_slots[0].modifier << "as a 2D texture; no EGL rung";
            release();
            return false;
          }
        }
      }
      // One GL texture per plane: the EGLImage behind each is re-targeted per
      // frame, but the texture ids themselves are created once here. The
      // external form needs exactly one, of a different target.
      const std::size_t texCount = m_external ? 1u : m_planeInfo.size();
      for(std::size_t p = 0; p < texCount; ++p)
      {
        m_glTexPlane[p] = m_external
                              ? EglDmaBufImporter::createExternalTexture()
                              : score::gfx::createLinearClampGlTexture2D();
        if(m_glTexPlane[p] == 0)
        {
          release();
          return false;
        }
      }
      m_glTex = m_glTexPlane[0];
      for(std::size_t i = 0; i < m_slots.size(); ++i)
      {
        const auto& s = m_slots[i];
        if(m_external)
        {
          std::uint32_t offs[3]{}, pitches[3]{};
          const auto n = std::min<std::uint32_t>(s.planeCount, 3);
          for(std::uint32_t p = 0; p < n; ++p)
          {
            offs[p] = s.offset + s.planes[p].offset;
            pitches[p] = s.planes[p].pitch;
          }
          if(!m_egl.importExternal(
                 m_eglSlots[i][0], m_glTexPlane[0], s.fd, s.modifier, offs,
                 pitches, n, m_wholeFrameFourcc, m_planeInfo[0].width,
                 m_wholeFrameHeight))
          {
            qDebug() << "DmaBufImportCapture: external EGL import refused slot"
                     << i << "fourcc" << Qt::hex << m_wholeFrameFourcc;
            release();
            return false;
          }
          continue;
        }
        for(std::size_t p = 0; p < m_planeInfo.size(); ++p)
        {
          const auto& pi = m_planeInfo[p];
          if(!m_egl.importPlane(
                 m_eglSlots[i][p], m_glTexPlane[p], s.fd, s.modifier,
                 s.offset + pi.offset, pi.pitch, pi.fmt.drmFourcc, pi.width,
                 pi.height))
          {
            qDebug() << "DmaBufImportCapture: EGL import refused slot" << i
                     << "plane" << p << "fd" << s.fd << "fourcc" << Qt::hex
                     << pi.fmt.drmFourcc << "modifier" << s.modifier;
            release();
            return false;
          }
        }
      }
      if(m_external)
        m_name += "/NV12-OES";
      m_name += "/EGL";
    }

    if(m_backend == Backend::None)
      return false;

    // QGles2Texture::prepareCreate() derives the GL bind target from the flags
    // alone, and createFrom() goes through it too -- so an external image whose
    // QRhiTexture lacks ExternalOES binds as GL_TEXTURE_2D and samples nothing.
    const auto texFlags
        = m_external ? QRhiTexture::ExternalOES : QRhiTexture::Flag{};
    for(std::size_t p = 0; p < m_planeInfo.size(); ++p)
    {
      const auto& pi = m_planeInfo[p];
      m_planeTex[p]
          = cfg.rhi->newTexture(pi.qfmt, QSize{pi.width, pi.height}, 1, texFlags);
      if(!m_planeTex[p])
      {
        release();
        return false;
      }
    }
    m_tex = m_planeTex[0];
    // Bind slot 0 so the textures are complete before the passes are built.
    if(!bindSlot(0))
    {
      release();
      return false;
    }
    return true;
  }

  void release() override
  {
#if defined(SCORE_GFX_HAS_VK_DMABUF_IMPORT)
    if(m_backend == Backend::Vulkan)
      for(auto& slot : m_vkSlots)
        for(auto& p : slot)
          m_vk.cleanupPlane(p);
#endif
    if(m_backend == Backend::OpenGL)
    {
      for(auto& slot : m_eglSlots)
        for(auto& p : slot)
          m_egl.cleanupPlane(p);
      for(auto& t : m_glTexPlane)
      {
        if(t != 0)
        {
          if(auto* ctx = QOpenGLContext::currentContext())
            if(auto* funcs = ctx->extraFunctions())
              funcs->glDeleteTextures(1, &t);
          t = 0;
        }
      }
      m_glTex = 0;
    }
    for(auto& t : m_planeTex)
    {
      delete t;
      t = nullptr;
    }
    m_tex = nullptr;
    m_planeInfo.clear();
    m_backend = Backend::None;
    m_glTexBound = false;
    m_name = m_tag + "-dmabuf";
    m_publisher.reset();
    m_returns.store(0, std::memory_order_relaxed);
    m_held = -1;
    m_acquisitions = 0;
    m_retireN = 0;
  }

  /// Producer thread. Slot @p i becomes render-owned; any slot still waiting
  /// unconsumed is handed straight back through takeReturnedSlots().
  bool ingestFrame(std::size_t i) override
  {
    if(i >= m_slots.size())
      return false;
    const int displaced
        = m_publisher.pending.exchange(int(i), std::memory_order_acq_rel);
    if(displaced >= 0 && displaced != int(i))
      m_returns.fetch_or(1u << unsigned(displaced), std::memory_order_release);
    return true;
  }

  /// Producer thread. Bitmask of slots the renderer has finished with; the
  /// producer must give each of them back to its device.
  std::uint32_t takeReturnedSlots() noexcept
  {
    return m_returns.exchange(0, std::memory_order_acquire);
  }

  void acquireForRender() override
  {
    const int slot = m_publisher.consume();
    if(slot < 0 || std::size_t(slot) >= m_slots.size())
      return;

    ++m_acquisitions;
    if(m_held >= 0 && m_held != slot && m_retireN < kMaxSlots)
      m_retire[m_retireN++] = Retired{m_held, m_acquisitions};
    m_held = slot;
    bindSlot(std::size_t(slot));

    std::uint32_t freed = 0;
    std::size_t keep = 0;
    for(std::size_t i = 0; i < m_retireN; ++i)
    {
      if(m_acquisitions - m_retire[i].at >= m_retireDepth)
        freed |= 1u << unsigned(m_retire[i].slot);
      else
        m_retire[keep++] = m_retire[i];
    }
    m_retireN = keep;
    if(freed)
      m_returns.fetch_or(freed, std::memory_order_release);
  }

  bool supportsSlotSelection() const noexcept override { return true; }

  bool acquireSlotForRender(
      std::size_t slot, QRhiResourceUpdateBatch&, QRhiCommandBuffer*) override
  {
    if(slot >= m_slots.size())
      return false;
    m_held = int(slot);
    return bindSlot(slot);
  }

  void releaseAfterRender() override { }

private:
  enum class Backend
  {
    None,
    Vulkan,
    OpenGL,
  };

  /// Point the renderer-facing texture at slot @p i's imported image.
  bool bindSlot(std::size_t i)
  {
    if(!m_tex)
      return true;
#if defined(SCORE_GFX_HAS_VK_DMABUF_IMPORT)
    if(m_backend == Backend::Vulkan)
    {
      // Re-issuing createFrom bumps the texture generation, which is what makes
      // QRhi rewrite the descriptor and emit the layout transition whose cache
      // invalidation exposes the producer's new bytes.
      for(std::size_t p = 0; p < m_planeInfo.size(); ++p)
      {
        if(!m_planeTex[p]->createFrom(QRhiTexture::NativeTexture{
               quint64(m_vkSlots[i][p].image), VK_IMAGE_LAYOUT_UNDEFINED}))
          return false;
      }
      return true;
    }
#endif
    if(m_backend == Backend::OpenGL)
    {
      if(m_external)
      {
        if(!m_egl.bindExternal(m_glTexPlane[0], m_eglSlots[i][0]))
          return false;
        if(m_glTexBound)
          return true;
        m_glTexBound = m_planeTex[0]->createFrom(
            QRhiTexture::NativeTexture{quint64(m_glTexPlane[0]), 0});
        return m_glTexBound;
      }
      for(std::size_t p = 0; p < m_planeInfo.size(); ++p)
        if(!m_egl.bindPlane(m_glTexPlane[p], m_eglSlots[i][p]))
          return false;
      // The GL texture ids never change -- only the EGLImages behind them --
      // and QRhiGles2 resolves the id at draw time, so the wrap is set up once.
      if(m_glTexBound)
        return true;
      for(std::size_t p = 0; p < m_planeInfo.size(); ++p)
        if(!m_planeTex[p]->createFrom(
               QRhiTexture::NativeTexture{quint64(m_glTexPlane[p]), 0}))
          return false;
      m_glTexBound = true;
      return true;
    }
    return false;
  }

  struct Retired
  {
    int slot{-1};
    std::uint64_t at{};
  };

  VideoCaptureStrategyConfig cfg{};
  /// A planar frame is one dma-buf holding N planes back to back; this is the
  /// per-plane split derived from the decoder's texture list at init().
  struct PlaneInfo
  {
    QRhiTexture::Format qfmt{};
    DmaBufImportFormat fmt{};
    std::uint32_t offset{}; ///< from the start of the slot
    std::uint32_t pitch{};
    int width{};
    int height{};
  };

  /// NV12/P010 have 2, planar YUV has 3; nothing we import has more.
  static constexpr std::size_t kMaxPlanes = 3;

  std::vector<DmaBufSlotDesc> m_slots;
  std::vector<PlaneInfo> m_planeInfo;
  /// Set by the caller when the whole frame should go in as one external
  /// image: the DRM fourcc of the frame (NV12 etc.) and its full height, which
  /// plane 0's geometry does not give for a subsampled layout.
  std::uint32_t m_wholeFrameFourcc{};
  int m_wholeFrameHeight{};
  bool m_external{false};
  std::uint32_t m_planeBytes{};
  std::array<QRhiTexture*, kMaxPlanes> m_planeTex{};
  std::array<std::uint32_t, kMaxPlanes> m_glTexPlane{};
  std::string m_tag;
  std::string m_name;
  Backend m_backend{Backend::None};

  QRhiTexture* m_tex{};

#if defined(SCORE_GFX_HAS_VK_DMABUF_IMPORT)
  DMABufPlaneImporter m_vk;
  std::array<std::array<DMABufPlaneImporter::PlaneImport, kMaxPlanes>, kMaxSlots> m_vkSlots{};
#endif
  EglDmaBufImporter m_egl;
  std::array<std::array<EglDmaBufImporter::PlaneImport, kMaxPlanes>, kMaxSlots> m_eglSlots{};
  unsigned int m_glTex{0};
  bool m_glTexBound{false};

  CaptureSlotPublisher m_publisher;
  std::atomic<std::uint32_t> m_returns{0};

  int m_held{-1};
  std::uint64_t m_acquisitions{0};
  std::size_t m_retireDepth{3};
  std::array<Retired, kMaxSlots> m_retire{};
  std::size_t m_retireN{0};
};

} // namespace score::gfx::interop
#endif // __linux__
