#include <Gfx/Graph/interop/GpuDirectOutput.hpp>

#include <QDebug>

namespace score::gfx::interop
{

GpuDirectOutput::~GpuDirectOutput()
{
  release();
}

bool GpuDirectOutput::init(const GpuDirectOutputConfig& cfg)
{
  if(!cfg.rhi || !cfg.state || !cfg.sourceTexture || !cfg.encoderFactory
     || !cfg.registrar.registerSlot || !cfg.registrar.releaseSlot
     || cfg.frameByteSize == 0 || cfg.slotCount <= 0)
  {
    qWarning() << "GpuDirectOutput: invalid config";
    return false;
  }
  release();
  m_cfg = cfg;

  if(!cuda_p2p_available())
  {
    qDebug() << "GpuDirectOutput: GPUDirect RDMA not available";
    return false;
  }
  if(cuda_p2p_init(&m_cudaCtx) != CUDA_P2P_SUCCESS || !m_cudaCtx)
  {
    qWarning() << "GpuDirectOutput: cuda_p2p_init failed";
    return false;
  }

  GpuRingBufferConfig rcfg{
      cfg.rhi, m_cudaCtx, cfg.frameByteSize, cfg.slotCount, cfg.debugName};
  if(!m_ring.create(rcfg))
  {
    qWarning() << "GpuDirectOutput: GpuRingBuffer::create failed";
    release();
    return false;
  }

  m_fence = makeInteropFence(*cfg.rhi);
  if(!m_fence || !m_fence->init(*cfg.rhi, m_cudaCtx))
  {
    // Note: backends that don't need a real fence (D3D11, GL) return a
    // valid no-op fence; only Vulkan/D3D12 stubs fail here. If init
    // fails on those, the strategy isn't viable at all.
    qWarning() << "GpuDirectOutput: InteropFence init failed";
    release();
    return false;
  }

  ComputeRingDispatcherConfig dcfg{
      cfg.rhi,         cfg.state,           &m_ring,
      cfg.sourceTexture, cfg.width,           cfg.height,
      cfg.encoderFactory, cfg.colorConversion, m_fence.get()};
  if(!m_dispatcher.init(dcfg))
  {
    qWarning() << "GpuDirectOutput: ComputeRingDispatcher::init failed";
    release();
    return false;
  }

  // Bounce + vendor register pass — one call per slot. The vendor pins the
  // CUDA-owned bounce buffer, never the ring's graphics-API buffer:
  // nvidia_p2p_get_pages (behind DMABufferLock(inRDMA=true) et al) only
  // accepts CUDA-allocator VA ranges. prepareNextFrame() bridges the gap
  // with one VRAM->VRAM copy. Track successes so a partial-failure
  // rollback unpins only the slots we pinned.
  m_bounce.assign(m_ring.slotCount(), nullptr);
  m_pinned.assign(m_ring.slotCount(), false);
  for(std::size_t i = 0; i < m_ring.slotCount(); ++i)
  {
    if(cuda_p2p_alloc_buffer(m_cudaCtx, m_cfg.frameByteSize, &m_bounce[i])
           != CUDA_P2P_SUCCESS
       || !m_bounce[i])
    {
      qWarning() << "GpuDirectOutput: bounce alloc failed at slot" << i << ":"
                 << cuda_p2p_get_error_string(m_cudaCtx);
      release();
      return false;
    }
    if(!m_cfg.registrar.registerSlot(m_bounce[i], m_cfg.frameByteSize))
    {
      qWarning() << "GpuDirectOutput: vendor registerSlot failed at slot" << i;
      release();
      return false;
    }
    m_pinned[i] = true;

    // One-time transfer probe on the first pinned slot: pinning only proves
    // the pages were accepted, not that the PCIe path permits P2P in the
    // playout direction. Bail cleanly here so the strategy chain falls back
    // rather than dropping every frame at submit time.
    if(i == 0 && m_cfg.registrar.verifyTransfer
       && !m_cfg.registrar.verifyTransfer(m_bounce[0], m_cfg.frameByteSize))
    {
      qWarning() << "GpuDirectOutput: vendor transfer probe failed — the "
                    "card↔GPU PCIe path does not permit P2P playout "
                    "(pin succeeded but DMA does not). Falling back.";
      release();
      return false;
    }
  }
  return true;
}

void GpuDirectOutput::release()
{
  // Vendor release: unpin the slots we successfully pinned, in reverse
  // order so the vendor sees a clean LIFO if it cares. Then free the
  // bounce buffers (only after the vendor let go of them).
  for(std::size_t i = m_pinned.size(); i-- > 0;)
  {
    if(m_pinned[i] && m_cfg.registrar.releaseSlot && i < m_bounce.size())
    {
      m_cfg.registrar.releaseSlot(m_bounce[i], m_cfg.frameByteSize);
    }
  }
  m_pinned.clear();
  for(void* b : m_bounce)
  {
    if(b)
      cuda_p2p_free_buffer(m_cudaCtx, b);
  }
  m_bounce.clear();

  m_dispatcher.release();

  if(m_fence)
  {
    m_fence->release();
    m_fence.reset();
  }
  m_ring.destroy();
  if(m_cudaCtx)
  {
    cuda_p2p_shutdown(m_cudaCtx);
    m_cudaCtx = nullptr;
  }
  m_fenceValue = 0;
  m_cfg = {};
}

void GpuDirectOutput::encodeFrame(QRhiCommandBuffer& cb)
{
  if(!valid())
    return;
  m_dispatcher.encode(cb, ++m_fenceValue);
}

void* GpuDirectOutput::prepareNextFrame()
{
  if(!valid())
    return nullptr;
  if(!m_dispatcher.waitOnCuda(m_fenceValue))
    return nullptr;
  // The encoder's output is now visible (fence / glFinish above); bridge
  // it into the vendor-pinned bounce buffer. copy_dtod stream-syncs, so
  // the vendor may DMA the moment we return.
  const std::size_t idx = m_ring.writeIndex();
  if(idx >= m_bounce.size() || !m_bounce[idx])
    return nullptr;
  if(cuda_p2p_copy_dtod(
         m_cudaCtx, m_bounce[idx], m_dispatcher.finishedSlot().gpuDevicePtr,
         m_cfg.frameByteSize)
     != CUDA_P2P_SUCCESS)
  {
    qWarning() << "GpuDirectOutput: bounce copy failed:"
               << cuda_p2p_get_error_string(m_cudaCtx);
    return nullptr;
  }
  return m_bounce[idx];
}

void GpuDirectOutput::advance()
{
  m_dispatcher.advance();
}

} // namespace score::gfx::interop
