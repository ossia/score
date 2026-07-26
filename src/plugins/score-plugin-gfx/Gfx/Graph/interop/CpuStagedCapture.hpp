#pragma once

/**
 * @file CpuStagedCapture.hpp
 * @brief Generic CPU-staging (host-staged) video capture strategy.
 *
 * The portable capture fallback shared by every vendor: the card DMAs each
 * arrived frame into one of a small fixed ring of page-locked host buffers;
 * the render thread uploads the freshest slot into the decoder's input texture.
 * Works on every QRhi backend.
 *
 * A `Policy` supplies the small per-vendor deltas so this one implementation
 * replaces the five byte-for-byte-divergent-only-in-name vendor copies:
 *   - `has_dma_lock` : page-lock each slot for the card's DMA engine (AJA
 *     `DMABufferLock(inRDMA=false)`); the CPU-only vendors leave it false.
 *   - `has_gl_fast_path` : on the GL backend, upload via a single raw
 *     `glTexSubImage2D` (one driver copy) instead of the portable
 *     `QRhiResourceUpdateBatch::uploadTexture` (which stages an extra
 *     full-frame memcpy). Only AJA uses it today.
 *
 * The upload row stride is `frameByteSize / height` universally: for the
 * CPU-only vendors this is the card raster's true (possibly v210-padded) pitch;
 * for AJA the slot is tightly packed so it equals width*4 — identical to the
 * value the standalone AJA shim used.
 */

#include <Gfx/Graph/interop/CaptureStrategyCommon.hpp>
#include <Gfx/Graph/interop/VideoCaptureStrategy.hpp>
#include <Gfx/Graph/interop/VkHostImportUpload.hpp>

#include <QtGui/private/qrhi_p.h>

#if QT_CONFIG(opengl)
#include <Gfx/Graph/interop/GLCaptureUpload.hpp>

#include <QtGui/private/qrhigles2_p.h>
#include <QOpenGLContext>
#endif

#include <QDebug>

#include <array>
#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>

namespace score::gfx::interop
{

/**
 * @brief Default policy: a pure sysmem ring with no card DMA page-lock and no
 *        raw-GL fast path. Used directly (via a `using` alias) by the CPU-only
 *        vendor captures — each supplies its own `fixed_name`.
 */
struct CpuStagedNoLockPolicy
{
  static constexpr bool has_dma_lock = false;
  static constexpr bool has_gl_fast_path = false;
  static constexpr const char* fixed_name = "CPU";
};

template <typename Policy>
struct CpuStagedCapture final : VideoCaptureStrategy
{
  CpuStagedCapture() = default;
  /// Forward constructor args to the policy (e.g. AJA's card + pixel format).
  template <typename A, typename... Rest>
  explicit CpuStagedCapture(A&& a, Rest&&... rest)
      : m_policy{std::forward<A>(a), std::forward<Rest>(rest)...}
  {
  }

  Policy m_policy{};
  VideoCaptureStrategyConfig cfg{};

  static constexpr std::size_t kSlotCount = 3;
  /// Raw rather than std::vector because the Vulkan host-import path needs the
  /// pages page-aligned; the allocation is freed in release().
  std::array<std::uint8_t*, kSlotCount> m_slots{};
  std::array<bool, kSlotCount> m_dmaLocked{};
  CaptureSlotPublisher m_publisher;

  /// Non-null only on Vulkan with VK_EXT_external_memory_host: the slots are
  /// imported as VkDeviceMemory and the GPU DMAs straight out of them, so the
  /// per-frame staging copy disappears entirely.
  VkHostImportUpload m_hostImport;

#if QT_CONFIG(opengl)
  // Non-null when the raw-GL fast path is engaged (GL backend, not forced
  // portable). Only ever set when Policy::has_gl_fast_path.
  QOpenGLContext* m_glCtx{};
#endif

  const char* name() const noexcept override
  {
    if constexpr(Policy::has_gl_fast_path)
    {
#if QT_CONFIG(opengl)
      return m_glCtx ? Policy::gl_engaged_name : Policy::gl_fallback_name;
#else
      return Policy::gl_fallback_name;
#endif
    }
    else
    {
      return Policy::fixed_name;
    }
  }

  bool init(const VideoCaptureStrategyConfig& c) override
  {
    cfg = c;
    if(!cfg.rhi || !cfg.outputTexture)
      return false;

    if constexpr(Policy::has_dma_lock)
    {
      // AJA: needs the card handle + an exact texture/frame byte match (the
      // slot is DMA'd into a tightly-packed buffer sampled 1:1 by the decoder).
      if(!m_policy.valid())
        return false;
      if(!validateCaptureTextureBytes(
             cfg.outputTexture, cfg.frameByteSize, Policy::log_tag))
        return false;
    }
    else
    {
      if(cfg.frameByteSize == 0)
        return false;
    }

#if QT_CONFIG(opengl)
    if constexpr(Policy::has_gl_fast_path)
    {
      if(cfg.rhi->backend() == QRhi::OpenGLES2
         && !qEnvironmentVariableIsSet(Policy::force_portable_env))
      {
        if(auto* native = static_cast<const QRhiGles2NativeHandles*>(
               cfg.rhi->nativeHandles());
           native && native->context)
          m_glCtx = native->context;
      }
    }
#endif

    // Align to the import requirement when it is available so the same slots
    // can serve both paths; otherwise plain max_align is enough.
    // SCORE_GFX_NO_VK_HOST_IMPORT forces the portable uploadTexture path so one
    // binary can measure both sides of the comparison.
    const std::size_t importAlign
        = qEnvironmentVariableIsSet("SCORE_GFX_NO_VK_HOST_IMPORT")
              ? 0u
              : VkHostImportUpload::requiredAlignment(*cfg.rhi);
    const std::size_t slotAlign = importAlign ? importAlign : alignof(std::max_align_t);

    for(std::size_t i = 0; i < kSlotCount; ++i)
    {
      m_slots[i] = static_cast<std::uint8_t*>(
          alignedSlotAlloc(cfg.frameByteSize, slotAlign));
      if(!m_slots[i])
      {
        release();
        return false;
      }
      std::memset(m_slots[i], 0, cfg.frameByteSize);
      if constexpr(Policy::has_dma_lock)
      {
        // Page-lock (paged, not RDMA) so the card's DMA into the host buffer
        // doesn't re-pin pages every frame.
        if(m_policy.dmaLock(m_slots[i], cfg.frameByteSize))
          m_dmaLocked[i] = true;
      }
    }

    if(importAlign)
    {
      std::vector<void*> ptrs;
      ptrs.reserve(kSlotCount);
      for(auto* p : m_slots)
        ptrs.push_back(p);
      if(m_hostImport.init(*cfg.rhi, ptrs, cfg.frameByteSize))
        qDebug() << "DMA capture: Vulkan host-import upload engaged (zero-copy)";
      else
        m_hostImport.release();
    }
    return true;
  }

  void release() override
  {
    m_hostImport.release();
    for(std::size_t i = 0; i < kSlotCount; ++i)
    {
      // NB: no DMA unlock here — the vendor capture session's close() does the
      // unlock-all, and by teardown the card object may already be gone.
      m_dmaLocked[i] = false;
      alignedSlotFree(m_slots[i]);
      m_slots[i] = nullptr;
    }
#if QT_CONFIG(opengl)
    if constexpr(Policy::has_gl_fast_path)
      m_glCtx = nullptr;
#endif
    m_publisher.reset();
  }

  std::size_t slotCount() const noexcept override { return kSlotCount; }

  void* slotBuffer(std::size_t i) const noexcept override
  {
    return (i < kSlotCount) ? m_slots[i] : nullptr;
  }

  bool ingestFrame(std::size_t i) override
  {
    if(i >= kSlotCount)
      return false;
    m_publisher.publish(i);
    return true;
  }

  QRhiTexture* outputTexture() const noexcept override { return cfg.outputTexture; }

  // Raw-API path unused: the upload goes through the batch overload below.
  void acquireForRender() override { }

  void acquireForRender(QRhiResourceUpdateBatch& res) override
  {
    acquireForRender(res, nullptr);
  }

  void acquireForRender(QRhiResourceUpdateBatch& res, QRhiCommandBuffer* cb) override
  {
    const int slotIdx = m_publisher.consume();
    if(slotIdx < 0 || static_cast<std::size_t>(slotIdx) >= kSlotCount)
      return;
    const void* src = m_slots[static_cast<std::size_t>(slotIdx)];

    // Vulkan zero-copy: the slot pages are already imported, so all that is
    // left is the device-side DMA. Needs the command buffer, hence the overload.
    if(cb && m_hostImport.valid())
    {
      const auto pxsz = cfg.outputTexture->pixelSize();
      if(m_hostImport.copyToTexture(
             *cb, *cfg.outputTexture, static_cast<std::size_t>(slotIdx),
             pxsz.width(), pxsz.height()))
        return;
    }

#if QT_CONFIG(opengl)
    if constexpr(Policy::has_gl_fast_path)
    {
      if(m_glCtx)
      {
        // Raw-GL fast path: one glTexSubImage2D, no staging copy.
        uploadClientToGLTexture(
            *m_glCtx, *cfg.outputTexture, src, m_policy.bgra());
        return;
      }
    }
#endif

    // Portable path: backend-neutral QRhi upload.
    QRhiTextureSubresourceUploadDescription sub(src, cfg.frameByteSize);
    if constexpr(Policy::has_dma_lock)
    {
      // AJA: the slot is tightly packed to the texture's byte layout
      // (validateCaptureTextureBytes enforced frameByteSize == texW*4*texH);
      // stride is texWidth*4, exactly as the standalone AJA shim computed it.
      const auto pxsz = cfg.outputTexture->pixelSize();
      sub.setDataStride(static_cast<quint32>(pxsz.width()) * 4u);
    }
    else
    {
      // CPU-only vendors: source stride = the card raster's true pitch (v210
      // rows are 48-pixel-group padded, wider than texWidth*4 when w % 48 != 0,
      // e.g. 720p — the texture-width stride sheared those frames).
      sub.setDataStride(
          cfg.height > 0 ? cfg.frameByteSize / static_cast<quint32>(cfg.height)
                         : 0);
    }
    res.uploadTexture(
        cfg.outputTexture,
        QRhiTextureUploadDescription{QRhiTextureUploadEntry{0, 0, sub}});
  }

  void releaseAfterRender() override { }
};

} // namespace score::gfx::interop
