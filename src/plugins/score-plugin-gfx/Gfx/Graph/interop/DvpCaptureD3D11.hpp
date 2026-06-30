#pragma once
#include <Gfx/Graph/interop/GpuDirectCaptureStrategy.hpp>
#include <nv_dvp_bridge.h>

#include <QtGui/private/qrhid3d11_p.h>

#include <QDebug>

#include <d3d11.h>

#include <array>
#include <cstdint>
#include <cstring>

namespace score::gfx::interop
{

/**
 * @brief D3D11 capture strategy via NVIDIA "GPUDirect for Video" (DVP).
 *
 * D3D11 sibling of DvpCaptureGl. Vendor-neutral: parameterized by the @p Lock
 * page-lock policy + the @p dvpFormat. See DvpCaptureGl for the full model.
 */
template <class Lock>
struct DvpCaptureD3D11 : score::gfx::interop::GpuDirectCaptureStrategy
{
  DvpCaptureD3D11(Lock lock, NvDvpFormat dvpFormat, const char* name) noexcept
      : m_lock{lock}, m_dvpFormat{dvpFormat}, m_name{name} {}

  Lock m_lock;
  NvDvpFormat m_dvpFormat{};
  const char* m_name{"DVP-D3D11"};

  score::gfx::interop::GpuDirectCaptureStrategyConfig cfg{};

  ID3D11Device* m_dev{};

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
        = static_cast<const QRhiD3D11NativeHandles*>(cfg.rhi->nativeHandles());
    if(!native || !native->dev)
      return false;
    m_dev = static_cast<ID3D11Device*>(native->dev);

    qDebug() << "DVP-IN(D3D11): loading dvp.dll...";
    if(nv_dvp_init_d3d11(&m_dvpCtx, m_dev) != NV_DVP_SUCCESS || !m_dvpCtx)
    {
      qWarning() << "DVP-IN(D3D11): init failed:"
                 << nv_dvp_get_error_string(m_dvpCtx);
      return false;
    }
    if(nv_dvp_thread_begin(m_dvpCtx) != NV_DVP_SUCCESS)
    {
      qWarning() << "DVP-IN(D3D11): thread_begin failed:"
                 << nv_dvp_get_error_string(m_dvpCtx);
      release();
      return false;
    }
    m_threadStarted = true;
    qDebug() << "DVP-IN(D3D11): dvp.dll loaded + D3D11 device bound";

    // Geometry from caller's QRhi texture; sysmem stride/size derived
    // and validated against the card's frame byte size.
    const QSize texSize = cfg.outputTexture->pixelSize();
    m_texW = texSize.width();
    m_texH = texSize.height();
    m_sysmemStrideBytes = static_cast<uint32_t>(m_texW) * 4u;
    m_sysmemBytes = m_sysmemStrideBytes * static_cast<uint32_t>(m_texH);

    if(m_sysmemBytes != cfg.frameByteSize)
    {
      qWarning() << "DVP-IN(D3D11): texture byte size" << m_sysmemBytes
                 << "(" << m_texW << "x" << m_texH << " RGBA8/BGRA8) !="
                 << "frame size" << cfg.frameByteSize
                 << "- texture geometry doesn't match captured layout";
      release();
      return false;
    }

    auto nt = cfg.outputTexture->nativeTexture();
    if(!nt.object)
    {
      qWarning() << "DVP-IN(D3D11): output texture has no native handle";
      release();
      return false;
    }
    auto* d3d11Tex = reinterpret_cast<ID3D11Texture2D*>(nt.object);

    const NvDvpFormat dvpFmt = m_dvpFormat;

    if(nv_dvp_register_d3d11_texture(
           m_dvpCtx, d3d11Tex, dvpFmt, uint32_t(m_texW), uint32_t(m_texH),
           &m_dvpTex)
       != NV_DVP_SUCCESS)
    {
      qWarning() << "DVP-IN(D3D11): register_d3d11_texture failed:"
                 << nv_dvp_get_error_string(m_dvpCtx);
      release();
      return false;
    }

    // Sysmem slots: page-locked (vendor policy) for the card's DMA + DVP-
    // registered for GPU DMA. The card's frame size (cfg.frameByteSize) is
    // what gets written in; the DVP transfer copies the same byte count out
    // to the texture.
    for(auto& slot : m_slots)
    {
      slot.sysmem = nv_dvp_aligned_alloc(m_sysmemBytes);
      if(!slot.sysmem)
      {
        qWarning() << "DVP-IN(D3D11): nv_dvp_aligned_alloc(" << m_sysmemBytes
                   << ") failed";
        release();
        return false;
      }
      std::memset(slot.sysmem, 0, m_sysmemBytes);
      if(!m_lock.lock(slot.sysmem, m_sysmemBytes))
      {
        qWarning() << "DVP-IN(D3D11): DMABufferLock(slot) failed";
        release();
        return false;
      }
      slot.dmaLocked = true;
      if(nv_dvp_register_sysmem_buffer(
             m_dvpCtx, slot.sysmem, dvpFmt, uint32_t(m_texW),
             uint32_t(m_texH), m_sysmemStrideBytes, &slot.dvpBuf)
         != NV_DVP_SUCCESS)
      {
        qWarning() << "DVP-IN(D3D11): register_sysmem_buffer failed:"
                   << nv_dvp_get_error_string(m_dvpCtx);
        release();
        return false;
      }
    }

    return true;
  }

  void release() override
  {
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
    m_dev = nullptr;
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
      qWarning() << "DVP-IN(D3D11): copy_buffer_to_texture failed:"
                 << nv_dvp_get_error_string(m_dvpCtx);
      return false;
    }
    return true;
  }

  QRhiTexture* outputTexture() const noexcept override { return cfg.outputTexture; }

  void acquireForRender() override
  {
    if(m_dvpCtx && m_dvpTex)
    {
      if(nv_dvp_acquire_texture(m_dvpCtx, m_dvpTex) != NV_DVP_SUCCESS)
      {
        qWarning() << "DVP-IN(D3D11): acquire_texture failed:"
                   << nv_dvp_get_error_string(m_dvpCtx);
      }
    }
  }

  void releaseAfterRender() override
  {
    if(m_dvpCtx && m_dvpTex)
    {
      if(nv_dvp_release_texture(m_dvpCtx, m_dvpTex) != NV_DVP_SUCCESS)
      {
        qWarning() << "DVP-IN(D3D11): release_texture failed:"
                   << nv_dvp_get_error_string(m_dvpCtx);
      }
    }
  }
};

} // namespace score::gfx::interop
