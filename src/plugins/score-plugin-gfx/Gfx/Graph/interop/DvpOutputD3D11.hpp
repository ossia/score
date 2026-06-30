#pragma once
#include <Gfx/Graph/interop/GpuDirectStrategy.hpp>
#include <Gfx/Graph/encoders/GPUVideoEncoder.hpp>
#include <nv_dvp_bridge.h>

#include <QtGui/private/qrhid3d11_p.h>

#include <QDebug>

#include <d3d11.h>

#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>

namespace score::gfx::interop
{

/**
 * @brief D3D11 output strategy via NVIDIA "GPUDirect for Video" (DVP).
 *
 * D3D11 sibling of DvpOutputGl. Vendor-neutral: parameterized by the @p Lock
 * page-lock policy + the encoder closures (@p makeEncoder / @p getOutputTexture).
 * See DvpOutputGl for the full per-frame model.
 */
template <class Lock>
struct DvpOutputD3D11 : score::gfx::interop::GpuDirectStrategy
{
  using MakeEncoder = std::function<std::unique_ptr<score::gfx::GPUVideoEncoder>()>;
  using GetOutputTexture
      = std::function<QRhiTexture*(score::gfx::GPUVideoEncoder*)>;

  DvpOutputD3D11(
      Lock lock, MakeEncoder makeEncoder, GetOutputTexture getOutputTexture,
      const char* name) noexcept
      : m_lock{lock}
      , m_makeEncoder{std::move(makeEncoder)}
      , m_getOutputTexture{std::move(getOutputTexture)}
      , m_name{name}
  {
  }

  Lock m_lock;
  MakeEncoder m_makeEncoder;
  GetOutputTexture m_getOutputTexture;
  const char* m_name{"DVP-D3D11"};

  score::gfx::interop::GpuDirectStrategyConfig cfg{};

  ID3D11Device* m_dev{};

  NvDvpContextHandle m_dvpCtx{};
  NvDvpResourceHandle m_dvpTex{};
  NvDvpResourceHandle m_dvpBuf{};
  bool m_threadStarted{};

  std::unique_ptr<score::gfx::GPUVideoEncoder> m_encoder;
  QRhiTexture* m_encoderOutput{}; /* non-owning; encoder owns it */

  void* m_sysmem{};
  uint32_t m_sysmemBytes{};
  uint32_t m_sysmemStrideBytes{};
  uint32_t m_sysmemRows{};
  bool m_dmaLocked{};

  const char* name() const noexcept override { return m_name; }

  static bool isSupported(QRhi* rhi) { return rhi != nullptr; }

  bool init(const score::gfx::interop::GpuDirectStrategyConfig& c) override
  {
    cfg = c;
    if(!isSupported(cfg.rhi) || !cfg.state || !m_lock.valid())
      return false;

    auto* native
        = static_cast<const QRhiD3D11NativeHandles*>(cfg.rhi->nativeHandles());
    if(!native || !native->dev)
      return false;
    m_dev = static_cast<ID3D11Device*>(native->dev);

    qDebug() << "DVP(D3D11): loading dvp.dll...";
    if(nv_dvp_init_d3d11(&m_dvpCtx, m_dev) != NV_DVP_SUCCESS || !m_dvpCtx)
    {
      qWarning() << "DVP(D3D11): init failed:"
                 << nv_dvp_get_error_string(m_dvpCtx);
      return false;
    }
    qDebug() << "DVP(D3D11): dvp.dll loaded + D3D11 device bound";

    if(nv_dvp_thread_begin(m_dvpCtx) != NV_DVP_SUCCESS)
    {
      qWarning() << "DVP(D3D11): thread_begin failed:"
                 << nv_dvp_get_error_string(m_dvpCtx);
      nv_dvp_shutdown(m_dvpCtx);
      m_dvpCtx = nullptr;
      return false;
    }
    m_threadStarted = true;

    auto colorShader = score::gfx::colorMatrixOut(
        AVCOL_SPC_BT709, AVCOL_TRC_BT709, AVCOL_RANGE_MPEG, AVCOL_PRI_BT709);

    m_encoder = m_makeEncoder();
    if(!m_encoder)
    {
      qWarning() << "DVP(D3D11): no fragment encoder for format";
      release();
      return false;
    }
    m_encoder->init(
        *cfg.rhi, *cfg.state, cfg.sourceTexture, cfg.width, cfg.height,
        colorShader);
    m_encoderOutput = m_getOutputTexture(m_encoder.get());
    if(!m_encoderOutput)
    {
      qWarning() << "DVP(D3D11): encoder did not produce an output texture";
      release();
      return false;
    }

    const QSize texSize = m_encoderOutput->pixelSize();
    m_sysmemRows = uint32_t(texSize.height());
    m_sysmemStrideBytes = uint32_t(texSize.width()) * 4u;
    m_sysmemBytes = m_sysmemStrideBytes * m_sysmemRows;

    if(m_sysmemBytes != cfg.frameByteSize)
    {
      qWarning() << "DVP(D3D11): encoder output size" << m_sysmemBytes
                 << "does not match frame size" << cfg.frameByteSize
                 << "- aborting";
      release();
      return false;
    }

    m_sysmem = nv_dvp_aligned_alloc(m_sysmemBytes);
    if(!m_sysmem)
    {
      qWarning() << "DVP(D3D11): nv_dvp_aligned_alloc(" << m_sysmemBytes
                 << ") failed";
      release();
      return false;
    }
    std::memset(m_sysmem, 0, m_sysmemBytes);

    if(!m_lock.lock(m_sysmem, m_sysmemBytes))
    {
      qWarning() << "DVP(D3D11): DMABufferLock(sysmem) failed";
      release();
      return false;
    }
    m_dmaLocked = true;

    auto nt = m_encoderOutput->nativeTexture();
    if(!nt.object)
    {
      qWarning() << "DVP(D3D11): encoder texture has no native handle";
      release();
      return false;
    }
    auto* d3d11Tex = reinterpret_cast<ID3D11Texture2D*>(nt.object);

    if(nv_dvp_register_d3d11_texture(
           m_dvpCtx, d3d11Tex, NV_DVP_FORMAT_RGBA8,
           uint32_t(texSize.width()), uint32_t(texSize.height()), &m_dvpTex)
       != NV_DVP_SUCCESS)
    {
      qWarning() << "DVP(D3D11): register_d3d11_texture failed:"
                 << nv_dvp_get_error_string(m_dvpCtx);
      release();
      return false;
    }

    if(nv_dvp_register_sysmem_buffer(
           m_dvpCtx, m_sysmem, NV_DVP_FORMAT_RGBA8, uint32_t(texSize.width()),
           uint32_t(texSize.height()), m_sysmemStrideBytes, &m_dvpBuf)
       != NV_DVP_SUCCESS)
    {
      qWarning() << "DVP(D3D11): register_sysmem_buffer failed:"
                 << nv_dvp_get_error_string(m_dvpCtx);
      release();
      return false;
    }

    return true;
  }

  void release() override
  {
    if(m_dvpCtx)
    {
      if(m_dvpTex)
      {
        nv_dvp_unregister(m_dvpCtx, m_dvpTex);
        m_dvpTex = nullptr;
      }
      if(m_dvpBuf)
      {
        nv_dvp_unregister(m_dvpCtx, m_dvpBuf);
        m_dvpBuf = nullptr;
      }
      if(m_threadStarted)
      {
        nv_dvp_thread_end(m_dvpCtx);
        m_threadStarted = false;
      }
      nv_dvp_shutdown(m_dvpCtx);
      m_dvpCtx = nullptr;
    }
    if(m_encoder)
    {
      m_encoder->release();
      m_encoder.reset();
    }
    m_encoderOutput = nullptr;

    if(m_dmaLocked && m_sysmem)
    {
      m_lock.unlock(m_sysmem, m_sysmemBytes);
      m_dmaLocked = false;
    }
    if(m_sysmem)
    {
      nv_dvp_aligned_free(m_sysmem);
      m_sysmem = nullptr;
    }
    m_dev = nullptr;
  }

  void encodeFrame(QRhiCommandBuffer& cb) override
  {
    if(nv_dvp_acquire_texture(m_dvpCtx, m_dvpTex) != NV_DVP_SUCCESS)
    {
      qWarning() << "DVP(D3D11): acquire_texture failed:"
                 << nv_dvp_get_error_string(m_dvpCtx);
    }
    m_encoder->exec(*cfg.rhi, cb);
  }

  void* prepareNextFrame() override
  {
    if(nv_dvp_release_texture(m_dvpCtx, m_dvpTex) != NV_DVP_SUCCESS)
    {
      qWarning() << "DVP(D3D11): release_texture failed:"
                 << nv_dvp_get_error_string(m_dvpCtx);
      return nullptr;
    }
    if(nv_dvp_copy_texture_to_buffer(m_dvpCtx, m_dvpTex, m_dvpBuf)
       != NV_DVP_SUCCESS)
    {
      qWarning() << "DVP(D3D11): copy_texture_to_buffer failed:"
                 << nv_dvp_get_error_string(m_dvpCtx);
      return nullptr;
    }
    return m_sysmem;
  }
};

} // namespace score::gfx::interop
