#include <Gfx/Graph/interop/HostFramePool.hpp>

#include <cstdlib>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace score::gfx::interop
{
namespace
{
#if defined(_WIN32)
constexpr std::size_t kRound = 65536;
#else
constexpr std::size_t kRound = 4096;
#endif

void* allocFrame(std::size_t bytes)
{
#if defined(_WIN32)
  return VirtualAlloc(nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
  return std::aligned_alloc(kRound, bytes);
#endif
}

void freeFrame(void* p)
{
#if defined(_WIN32)
  if(p)
    VirtualFree(p, 0, MEM_RELEASE);
#else
  std::free(p);
#endif
}
} // namespace

HostFramePool::HostFramePool() = default;

HostFramePool::~HostFramePool()
{
  release();
}

bool HostFramePool::allocate(
    std::size_t frameBytes, int frames, VendorDmaRegistrar reg)
{
  release();
  if(frameBytes == 0 || frames <= 0)
    return false;

  const std::size_t rounded = (frameBytes + kRound - 1) / kRound * kRound;
  m_reg = std::move(reg);
  m_frames.reserve(std::size_t(frames));
  for(int i = 0; i < frames; ++i)
  {
    void* p = allocFrame(rounded);
    if(!p
       || (m_reg.registerSlot
           && !m_reg.registerSlot(p, std::uint32_t(rounded))))
    {
      freeFrame(p);
      release();
      return false;
    }
    m_frames.push_back({p, rounded, false});
  }
  return true;
}

void HostFramePool::release()
{
  for(auto& f : m_frames)
  {
    if(m_reg.releaseSlot)
      m_reg.releaseSlot(f.base, std::uint32_t(f.bytes));
    freeFrame(f.base);
  }
  m_frames.clear();
  m_reg = {};
}

FrameMemoryProvider HostFramePool::provider()
{
  FrameMemoryProvider p;
  p.acquire = [this] { return acquire(); };
  p.cancel = [this](void* bytes) { recycle(bytes); };
  return p;
}

bool HostFramePool::owns(const void* p) const noexcept
{
  // Frame bases are immutable after allocate(); only inUse mutates under the
  // mutex, so ownership tests need no lock.
  for(const auto& f : m_frames)
    if(f.base == p)
      return true;
  return false;
}

void HostFramePool::recycle(void* p) noexcept
{
  std::lock_guard lock{m_mutex};
  for(auto& f : m_frames)
  {
    if(f.base == p)
    {
      f.inUse = false;
      return;
    }
  }
}

VendorFrameMemory HostFramePool::acquire() noexcept
{
  std::lock_guard lock{m_mutex};
  for(auto& f : m_frames)
  {
    if(!f.inUse)
    {
      f.inUse = true;
      return {f.base, f.base, f.bytes};
    }
  }
  return {};
}

} // namespace score::gfx::interop
