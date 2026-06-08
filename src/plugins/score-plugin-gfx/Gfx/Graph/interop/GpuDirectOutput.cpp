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

  // Vendor register pass — one call per slot. Track which ones succeeded
  // so a partial-failure rollback unpins only the slots we pinned.
  m_pinned.assign(m_ring.slotCount(), false);
  for(std::size_t i = 0; i < m_ring.slotCount(); ++i)
  {
    void* gpuPtr = m_ring.slot(i).gpuDevicePtr;
    if(!m_cfg.registrar.registerSlot(gpuPtr, m_cfg.frameByteSize))
    {
      qWarning() << "GpuDirectOutput: vendor registerSlot failed at slot" << i;
      release();
      return false;
    }
    m_pinned[i] = true;
  }
  return true;
}

void GpuDirectOutput::release()
{
  // Vendor release: unpin the slots we successfully pinned, in reverse
  // order so the vendor sees a clean LIFO if it cares.
  for(std::size_t i = m_pinned.size(); i-- > 0;)
  {
    if(m_pinned[i] && m_cfg.registrar.releaseSlot && i < m_ring.slotCount())
    {
      m_cfg.registrar.releaseSlot(
          m_ring.slot(i).gpuDevicePtr, m_cfg.frameByteSize);
    }
  }
  m_pinned.clear();

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
  return m_dispatcher.finishedSlot().gpuDevicePtr;
}

void GpuDirectOutput::advance()
{
  m_dispatcher.advance();
}

} // namespace score::gfx::interop
