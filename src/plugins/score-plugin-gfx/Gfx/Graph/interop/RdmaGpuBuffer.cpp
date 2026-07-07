#include <Gfx/Graph/interop/RdmaGpuBuffer.hpp>

#include <Gfx/Graph/interop/CudaFunctions.hpp>
#include <Gfx/Graph/interop/CudaVmmAllocator.hpp>
#include <Gfx/Graph/interop/VkExternalMemoryHelpers.hpp>

#include <QDebug>

#include <cstdint>
#include <new>
#include <vector>

namespace score::gfx::interop
{

namespace
{
constexpr std::size_t kRdmaAlign = 65536; // 64 KiB (RDMA + host fallback)

std::size_t roundUpRdma(std::size_t n, std::size_t a) noexcept
{
  return (n + a - 1) / a * a;
}
}

struct RdmaGpuBuffer::Impl
{
  RdmaGpuApi api{RdmaGpuApi::None};
  std::vector<RdmaGpuSlot> slots;

#if QT_HAS_VULKAN
  // Copied so destroy() can free the per-slot RdmaBuffer contexts.
  score::gfx::vkinterop::VulkanCtx vkCtx{};
#endif

  void releaseSlot(RdmaGpuSlot& s) noexcept
  {
    switch(api)
    {
      case RdmaGpuApi::Cuda:
        delete static_cast<CudaVmmAllocation*>(s.opaque);
        break;
      case RdmaGpuApi::Vulkan:
#if QT_HAS_VULKAN
        if(s.opaque)
        {
          auto* rb = static_cast<score::gfx::vkinterop::RdmaBuffer*>(s.opaque);
          score::gfx::vkinterop::destroyRdma(vkCtx, *rb);
          delete rb;
        }
#endif
        break;
      case RdmaGpuApi::HostFallback:
        if(s.gpuVA)
          ::operator delete(s.gpuVA, std::align_val_t{kRdmaAlign});
        break;
      case RdmaGpuApi::D3D12:
#if defined(SCORE_HAS_NVAPI)
        // The committed resource is held in opaque; release it.
        if(s.opaque)
          static_cast<IUnknown*>(s.opaque)->Release();
#endif
        break;
      case RdmaGpuApi::None:
        break;
    }
    s = {};
  }
};

RdmaGpuBuffer::RdmaGpuBuffer() noexcept = default;
RdmaGpuBuffer::~RdmaGpuBuffer()
{
  destroy();
}
RdmaGpuBuffer::RdmaGpuBuffer(RdmaGpuBuffer&&) noexcept = default;
RdmaGpuBuffer& RdmaGpuBuffer::operator=(RdmaGpuBuffer&& other) noexcept
{
  if(this != &other)
  {
    destroy();
    m_impl = std::move(other.m_impl);
  }
  return *this;
}

bool RdmaGpuBuffer::create(const RdmaGpuBufferConfig& cfg)
{
  destroy();
  if(cfg.slotCount == 0 || cfg.frameBytes == 0 || cfg.api == RdmaGpuApi::None)
    return false;

  auto impl = std::make_unique<Impl>();
  impl->api = cfg.api;
  impl->slots.resize(cfg.slotCount);

  bool ok = true;
  switch(cfg.api)
  {
    case RdmaGpuApi::Cuda:
    {
      if(!cfg.cuda)
      {
        ok = false;
        break;
      }
      for(std::uint32_t i = 0; i < cfg.slotCount && ok; ++i)
      {
        auto alloc = CudaVmmAllocator::allocate(
            *cfg.cuda, cfg.cudaDevice, cfg.frameBytes, /*exportable=*/false);
        if(!alloc.valid())
        {
          ok = false;
          break;
        }
        auto* heap = new CudaVmmAllocation(std::move(alloc));
        auto& s = impl->slots[i];
        s.gpuVA = reinterpret_cast<void*>(
            static_cast<std::uintptr_t>(heap->devicePointer()));
        s.size = heap->size();
        s.opaque = heap;
      }
      break;
    }

    case RdmaGpuApi::Vulkan:
    {
#if QT_HAS_VULKAN
      if(!cfg.vulkanCtx)
      {
        ok = false;
        break;
      }
      impl->vkCtx
          = *static_cast<const score::gfx::vkinterop::VulkanCtx*>(cfg.vulkanCtx);
      for(std::uint32_t i = 0; i < cfg.slotCount && ok; ++i)
      {
        auto buf = score::gfx::vkinterop::createRdmaBuffer(
            impl->vkCtx, static_cast<VkDeviceSize>(cfg.frameBytes));
        if(!buf)
        {
          ok = false;
          break;
        }
        auto* heap = new score::gfx::vkinterop::RdmaBuffer(*buf);
        auto& s = impl->slots[i];
        s.gpuVA = reinterpret_cast<void*>(
            static_cast<std::uintptr_t>(heap->remoteAddress));
        s.size = static_cast<std::size_t>(heap->size);
        s.opaque = heap;
      }
#else
      ok = false; // Vulkan not compiled in
#endif
      break;
    }

    case RdmaGpuApi::D3D12:
    {
      // NvAPI_D3D12_CreateCommittedRDMABuffer per slot (GPU VA -> gpuVA, resource
      // -> opaque). Scaffolded + gated: the NvAPI SDK isn't part of the score
      // build (it's only vendored under the Deltacast examples), so this stays
      // unavailable until SCORE_HAS_NVAPI is configured and the call is added.
      qWarning() << "RdmaGpuBuffer: D3D12/NvAPI backend not yet implemented";
      ok = false;
      break;
    }

    case RdmaGpuApi::HostFallback:
    {
      const std::size_t sz = roundUpRdma(cfg.frameBytes, kRdmaAlign);
      for(std::uint32_t i = 0; i < cfg.slotCount && ok; ++i)
      {
        void* p = ::operator new(
            sz, std::align_val_t{kRdmaAlign}, std::nothrow);
        if(!p)
        {
          ok = false;
          break;
        }
        auto& s = impl->slots[i];
        s.gpuVA = p;
        s.size = sz;
        s.opaque = nullptr;
      }
      break;
    }

    case RdmaGpuApi::None:
      ok = false;
      break;
  }

  if(!ok)
  {
    for(auto& s : impl->slots)
      impl->releaseSlot(s);
    return false;
  }

  m_impl = std::move(impl);
  return true;
}

bool RdmaGpuBuffer::valid() const noexcept
{
  return m_impl && !m_impl->slots.empty();
}

RdmaGpuApi RdmaGpuBuffer::backend() const noexcept
{
  return m_impl ? m_impl->api : RdmaGpuApi::None;
}

std::size_t RdmaGpuBuffer::slotCount() const noexcept
{
  return m_impl ? m_impl->slots.size() : 0;
}

const RdmaGpuSlot& RdmaGpuBuffer::slot(std::size_t i) const noexcept
{
  static const RdmaGpuSlot empty{};
  if(!m_impl || i >= m_impl->slots.size())
    return empty;
  return m_impl->slots[i];
}

void RdmaGpuBuffer::destroy() noexcept
{
  if(!m_impl)
    return;
  for(auto& s : m_impl->slots)
    m_impl->releaseSlot(s);
  m_impl.reset();
}

} // namespace score::gfx::interop
