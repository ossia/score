#pragma once
#include <Gfx/Graph/interop/GpuDirectCaptureStrategy.hpp>
#include <nv_dvp_bridge.h>

#include <QtGui/private/qrhigles2_p.h>
#include <QOpenGLContext>

#include <QDebug>

#include <array>
#include <cstdint>
#include <cstring>

namespace score::gfx::interop
{

/**
 * @brief OpenGL capture strategy via NVIDIA "GPUDirect for Video" (DVP).
 *
 * Vendor-neutral: the per-vendor part is the @p Lock policy (how the sysmem
 * slot is page-locked for the card's DMA engine) plus the @p dvpFormat (the
 * 4-byte texel layout of the decode-input texture). The capture loop writes
 * each frame into slotBuffer(i) (card DMA for AJA, CPU memcpy for DeckLink);
 * ingestFrame(i) then DVP-copies the slot to the output texture.
 *
 * The QRhi GL context must be current on the calling thread when init() runs
 * and when the renderer-side acquire/release runs.
 */
template <class Lock>
struct DvpCaptureGl : score::gfx::interop::GpuDirectCaptureStrategy
{
  DvpCaptureGl(Lock lock, NvDvpFormat dvpFormat, const char* name) noexcept
      : m_lock{lock}, m_dvpFormat{dvpFormat}, m_name{name} {}

  Lock m_lock;
  NvDvpFormat m_dvpFormat{};
  const char* m_name{"DVP-GL"};

  score::gfx::interop::GpuDirectCaptureStrategyConfig cfg{};

  QOpenGLContext* m_glCtx{};

  NvDvpContextHandle m_dvpCtx{};
  NvDvpResourceHandle m_dvpTex{};
  bool m_threadStarted{};

  static constexpr std::size_t kSlotCount = 3;
  struct Slot
  {
    void* sysmem{};
    NvDvpResourceHandle dvpBuf{};
    bool dmaLocked{};
  };
  std::array<Slot, kSlotCount> m_slots{};

  uint32_t m_sysmemBytes{};
  uint32_t m_sysmemStrideBytes{};
  int m_texW{};
  int m_texH{};

  const char* name() const noexcept override { return m_name; }

  bool init(const score::gfx::interop::GpuDirectCaptureStrategyConfig& c) override
  {
    cfg = c;
    if(!cfg.rhi || !m_lock.valid() || !cfg.outputTexture)
      return false;

    auto* native
        = static_cast<const QRhiGles2NativeHandles*>(cfg.rhi->nativeHandles());
    if(!native || !native->context)
      return false;
    m_glCtx = native->context;

    // dvpInitGLContext binds DVP to the *current* GL context, but init() runs
    // outside a QRhi frame so nothing is current here. Make the QRhi GL
    // context current on the render state's offscreen surface first. If this
    // fails, DVP init would fail anyway (and with a misleading error), so bail
    // out with a clear message.
    if(!cfg.state || !cfg.state->surface)
    {
      qWarning() << "DVP-IN(GL): no offscreen surface available to bind the "
                    "GL context; cannot init DVP";
      return false;
    }
    if(!m_glCtx->makeCurrent(cfg.state->surface))
    {
      qWarning() << "DVP-IN(GL): makeCurrent() failed; cannot bind DVP to "
                    "the GL context";
      return false;
    }

    // Validate everything that can fail cheaply BEFORE dvpInitGLContext:
    // closing a DVP GL binding poisons the context it was bound to, and on
    // an init-failure exit the caller keeps rendering on this context.
    const QSize texSize = cfg.outputTexture->pixelSize();
    m_texW = texSize.width();
    m_texH = texSize.height();
    m_sysmemStrideBytes = static_cast<uint32_t>(m_texW) * 4u;
    m_sysmemBytes = m_sysmemStrideBytes * static_cast<uint32_t>(m_texH);

    if(m_sysmemBytes != cfg.frameByteSize)
    {
      qWarning() << "DVP-IN(GL): texture byte size" << m_sysmemBytes
                 << "!=" << cfg.frameByteSize;
      return false;
    }

    auto nt = cfg.outputTexture->nativeTexture();
    if(!nt.object)
      return false;
    const uint32_t glTexId = uint32_t(nt.object);

    qDebug() << "DVP-IN(GL): loading dvp.dll...";
    if(nv_dvp_init_gl(&m_dvpCtx) != NV_DVP_SUCCESS || !m_dvpCtx)
    {
      qWarning() << "DVP-IN(GL): init failed:"
                 << nv_dvp_get_error_string(m_dvpCtx);
      return false;
    }
    if(nv_dvp_thread_begin(m_dvpCtx) != NV_DVP_SUCCESS)
    {
      release();
      return false;
    }
    m_threadStarted = true;
    qDebug() << "DVP-IN(GL): dvp.dll loaded + GL context bound";

    const NvDvpFormat dvpFmt = m_dvpFormat;

    if(nv_dvp_register_gl_texture(
           m_dvpCtx, glTexId, dvpFmt, uint32_t(m_texW), uint32_t(m_texH),
           &m_dvpTex)
       != NV_DVP_SUCCESS)
    {
      qWarning() << "DVP-IN(GL): register_gl_texture failed:"
                 << nv_dvp_get_error_string(m_dvpCtx);
      release();
      return false;
    }

    for(auto& slot : m_slots)
    {
      slot.sysmem = nv_dvp_aligned_alloc(m_sysmemBytes);
      if(!slot.sysmem)
      {
        release();
        return false;
      }
      std::memset(slot.sysmem, 0, m_sysmemBytes);
      if(!m_lock.lock(slot.sysmem, m_sysmemBytes))
      {
        release();
        return false;
      }
      slot.dmaLocked = true;
      if(nv_dvp_register_sysmem_buffer(
             m_dvpCtx, slot.sysmem, dvpFmt, uint32_t(m_texW),
             uint32_t(m_texH), m_sysmemStrideBytes, &slot.dvpBuf)
         != NV_DVP_SUCCESS)
      {
        release();
        return false;
      }
    }
    return true;
  }

  void release() override
  {
    // DVP requires its GL context current for unregister/close; nothing
    // guarantees that at destroyOutput time.
    if(m_dvpCtx && m_glCtx && cfg.state && cfg.state->surface
       && QOpenGLContext::currentContext() != m_glCtx)
      m_glCtx->makeCurrent(cfg.state->surface);

    if(m_dvpCtx)
    {
      for(auto& slot : m_slots)
      {
        if(slot.dvpBuf)
        {
          nv_dvp_unregister(m_dvpCtx, slot.dvpBuf);
          slot.dvpBuf = nullptr;
        }
        if(slot.dmaLocked)
        {
          m_lock.unlock(slot.sysmem, m_sysmemBytes);
          slot.dmaLocked = false;
        }
        if(slot.sysmem)
        {
          nv_dvp_aligned_free(slot.sysmem);
          slot.sysmem = nullptr;
        }
      }
      if(m_dvpTex)
      {
        nv_dvp_unregister(m_dvpCtx, m_dvpTex);
        m_dvpTex = nullptr;
      }
      if(m_threadStarted)
      {
        nv_dvp_thread_end(m_dvpCtx);
        m_threadStarted = false;
      }
      nv_dvp_shutdown(m_dvpCtx);
      m_dvpCtx = nullptr;
    }
    // Don't delete cfg.outputTexture — we don't own it.
    m_glCtx = nullptr;
  }

  std::size_t slotCount() const noexcept override { return kSlotCount; }
  void* slotBuffer(std::size_t i) const noexcept override
  {
    return i < kSlotCount ? m_slots[i].sysmem : nullptr;
  }

  bool ingestFrame(std::size_t i) override
  {
    if(i >= kSlotCount)
      return false;
    auto& slot = m_slots[i];
    if(!slot.dvpBuf || !m_dvpTex)
      return false;
    if(nv_dvp_copy_buffer_to_texture(m_dvpCtx, slot.dvpBuf, m_dvpTex)
       != NV_DVP_SUCCESS)
    {
      qWarning() << "DVP-IN(GL): copy_buffer_to_texture failed:"
                 << nv_dvp_get_error_string(m_dvpCtx);
      return false;
    }
    return true;
  }

  QRhiTexture* outputTexture() const noexcept override { return cfg.outputTexture; }

  void acquireForRender() override
  {
    if(m_dvpCtx && m_dvpTex)
      nv_dvp_acquire_texture(m_dvpCtx, m_dvpTex);
  }

  void releaseAfterRender() override
  {
    if(m_dvpCtx && m_dvpTex)
      nv_dvp_release_texture(m_dvpCtx, m_dvpTex);
  }
};

} // namespace score::gfx::interop
