#include <Gfx/Graph/interop/GpuRingBuffer.hpp>

#include <QDebug>

#if defined(_WIN32)
#include <QtGui/private/qrhid3d11_p.h>
#include <d3d11.h>
#endif

#include <QtGui/private/qrhigles2_p.h>

namespace score::gfx::interop
{

GpuRingBuffer::~GpuRingBuffer()
{
  destroy();
}

bool GpuRingBuffer::create(const GpuRingBufferConfig& cfg)
{
  if(!cfg.rhi || !cfg.cudaCtx || cfg.bufferSize == 0 || cfg.slotCount <= 0)
    return false;
  destroy();
  m_cfg = cfg;
  m_slots.resize(cfg.slotCount);
  m_writeIndex = 0;

  bool ok = false;
  switch(cfg.rhi->backend())
  {
#if defined(_WIN32)
    case QRhi::D3D11:
      ok = createD3D11();
      break;
    case QRhi::D3D12:
      ok = createD3D12Stub();
      break;
#endif
    case QRhi::OpenGLES2:
      ok = createOpenGL();
      break;
    case QRhi::Vulkan:
      ok = createVulkanStub();
      break;
    default:
      qWarning() << "GpuRingBuffer: unsupported QRhi backend"
                 << cfg.rhi->backend();
      break;
  }

  if(!ok)
    destroy();
  return ok;
}

void GpuRingBuffer::destroy()
{
  // Release CUDA mappings BEFORE destroying the underlying QRhi buffers
  // so the bridge can deregister the native resource cleanly.
  for(auto& slot : m_slots)
  {
    if(slot.cudaHandle && m_cfg.cudaCtx)
      cuda_p2p_release_buffer(m_cfg.cudaCtx, slot.cudaHandle);
    slot.cudaHandle = nullptr;
    slot.gpuDevicePtr = nullptr;
  }
  for(auto& slot : m_slots)
  {
    delete slot.qrhiBuffer;
    slot.qrhiBuffer = nullptr;
  }
  m_slots.clear();
  m_writeIndex = 0;
}

std::size_t GpuRingBuffer::advance() noexcept
{
  if(m_slots.empty())
    return 0;
  m_writeIndex = (m_writeIndex + 1) % m_slots.size();
  return m_writeIndex;
}

// =============================================================================
// D3D11 backend
// =============================================================================

bool GpuRingBuffer::createD3D11()
{
#if defined(_WIN32)
  auto* native = static_cast<const QRhiD3D11NativeHandles*>(
      m_cfg.rhi->nativeHandles());
  if(!native || !native->dev)
    return false;
  auto* d3d11Device = static_cast<ID3D11Device*>(native->dev);

  for(auto& slot : m_slots)
  {
    // QRhi-managed StorageBuffer. D3D11 backend creates with
    // D3D11_BIND_UNORDERED_ACCESS + ALLOW_RAW_VIEWS — what the
    // ComputeEncoder's writeonly std430 SSBO needs.
    slot.qrhiBuffer = m_cfg.rhi->newBuffer(
        QRhiBuffer::Static, QRhiBuffer::StorageBuffer, m_cfg.bufferSize);
    if(!slot.qrhiBuffer)
      return false;
    slot.qrhiBuffer->setName(QByteArray(m_cfg.debugName));
    if(!slot.qrhiBuffer->create())
      return false;

    // Extract native ID3D11Buffer*. For Static buffers the D3D11 backend
    // stores `&buffer` in objects[0].
    auto nb = slot.qrhiBuffer->nativeBuffer();
    if(nb.slotCount <= 0 || !nb.objects[0])
      return false;
    auto* d3d11Buf = *static_cast<ID3D11Buffer* const*>(nb.objects[0]);
    if(!d3d11Buf)
      return false;

    if(cuda_p2p_import_d3d11_buffer(
           m_cfg.cudaCtx, d3d11Buf, d3d11Device, m_cfg.bufferSize,
           &slot.gpuDevicePtr, &slot.cudaHandle)
           != CUDA_P2P_SUCCESS
       || !slot.gpuDevicePtr)
    {
      qWarning() << "GpuRingBuffer(D3D11): bridge import failed:"
                 << cuda_p2p_get_error_string(m_cfg.cudaCtx);
      return false;
    }
  }
  return true;
#else
  return false;
#endif
}

// =============================================================================
// OpenGL backend
// =============================================================================

bool GpuRingBuffer::createOpenGL()
{
  auto* native = static_cast<const QRhiGles2NativeHandles*>(
      m_cfg.rhi->nativeHandles());
  if(!native || !native->context)
    return false;

  for(auto& slot : m_slots)
  {
    slot.qrhiBuffer = m_cfg.rhi->newBuffer(
        QRhiBuffer::Static, QRhiBuffer::StorageBuffer, m_cfg.bufferSize);
    if(!slot.qrhiBuffer)
      return false;
    slot.qrhiBuffer->setName(QByteArray(m_cfg.debugName));
    if(!slot.qrhiBuffer->create())
      return false;

    auto nb = slot.qrhiBuffer->nativeBuffer();
    if(nb.slotCount <= 0 || !nb.objects[0])
      return false;
    const std::uint32_t glBufferId
        = *static_cast<const std::uint32_t*>(nb.objects[0]);
    if(glBufferId == 0)
      return false;

    if(cuda_p2p_import_gl_buffer(
           m_cfg.cudaCtx, glBufferId, m_cfg.bufferSize, &slot.gpuDevicePtr,
           &slot.cudaHandle)
           != CUDA_P2P_SUCCESS
       || !slot.gpuDevicePtr)
    {
      qWarning() << "GpuRingBuffer(GL): bridge import failed:"
                 << cuda_p2p_get_error_string(m_cfg.cudaCtx);
      return false;
    }
  }
  return true;
}

// =============================================================================
// Vulkan backend — blocked stub
// =============================================================================

bool GpuRingBuffer::createVulkanStub()
{
  qDebug() << "GpuRingBuffer(Vulkan): unsupported — QRhi VMA-allocates "
              "buffers without VkExportMemoryAllocateInfo, so "
              "cudaImportExternalMemory cannot pick up the underlying "
              "VkBuffer. Real impl requires either patching QVkBuffer via "
              "QRhi private API to inject the export-info chain, or a "
              "parallel externally-managed buffer + beginExternal compute "
              "dispatch path.";
  return false;
}

// =============================================================================
// D3D12 backend — blocked stub
// =============================================================================

bool GpuRingBuffer::createD3D12Stub()
{
  qDebug() << "GpuRingBuffer(D3D12): unsupported — QRhi D3D12 backend "
              "lacks SHARED heap support and createFrom(NativeBuffer). "
              "Dead-on-arrival as long as AJA's Windows driver "
              "doesn't expose inRDMA either (so D3D12 tier-3 is moot "
              "scaffolding for future SDKs).";
  return false;
}

} // namespace score::gfx::interop
