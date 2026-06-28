#include <Gfx/Graph/interop/PacedFramePump.hpp>

namespace score::gfx::interop
{

PacedFramePump::PacedFramePump(Hooks hooks, int ringDepth)
    : m_hooks{std::move(hooks)}
    , m_depth{ringDepth > 1 ? ringDepth : 2}
    , m_slots(static_cast<std::size_t>(m_depth))
{
  for(auto& s : m_slots)
    s.store(nullptr, std::memory_order_relaxed);
}

PacedFramePump::~PacedFramePump()
{
  stop();
}

void PacedFramePump::start()
{
  if(m_running.exchange(true))
    return;
  m_thread = std::thread{[this] { run(); }};
}

void PacedFramePump::stop()
{
  if(!m_running.exchange(false))
    return;
  if(m_thread.joinable())
    m_thread.join();
}

bool PacedFramePump::push(void* framePtr)
{
  if(!framePtr)
    return false;

  const uint32_t w = m_writeIdx.load(std::memory_order_relaxed);
  const uint32_t r = m_readIdx.load(std::memory_order_acquire);

  // Full check: leave one empty slot so writeIdx != readIdx unambiguously
  // means non-empty.
  if(w - r >= static_cast<uint32_t>(m_depth))
  {
    m_drops.fetch_add(1, std::memory_order_relaxed);
    return false;
  }

  m_slots[w % m_depth].store(framePtr, std::memory_order_relaxed);
  // Release-publish the slot so the consumer's acquire-load on writeIdx
  // happens-after the slot store.
  m_writeIdx.store(w + 1, std::memory_order_release);
  return true;
}

void PacedFramePump::run()
{
  while(m_running.load(std::memory_order_relaxed))
  {
    // Block until the next output tick (vendor hook). false = timeout; loop so
    // we re-check m_running for shutdown responsiveness.
    if(!m_hooks.waitForTick || !m_hooks.waitForTick())
      continue;

    if(!m_running.load(std::memory_order_relaxed))
      break;

    // Drain the ring of any slots produced since the last tick - we submit the
    // most recent one and drop earlier (older) ones rather than fall behind the
    // output clock. (For lipsync-strict cases this would be one-per-tick; first
    // cut prefers freshness.)
    const uint32_t w = m_writeIdx.load(std::memory_order_acquire);
    const uint32_t r = m_readIdx.load(std::memory_order_relaxed);
    if(w == r)
    {
      m_underruns.fetch_add(1, std::memory_order_relaxed);
      continue;
    }

    void* framePtr = nullptr;
    uint32_t consume = r;
    while(consume != w)
    {
      void* p = m_slots[consume % m_depth].load(std::memory_order_relaxed);
      m_slots[consume % m_depth].store(nullptr, std::memory_order_relaxed);
      if(p)
        framePtr = p; // keep the last-published one
      ++consume;
    }
    m_readIdx.store(consume, std::memory_order_release);

    if(!framePtr)
      continue;

    // Card-side back-pressure (optional vendor hook).
    if(m_hooks.canAccept && !m_hooks.canAccept())
    {
      m_drops.fetch_add(1, std::memory_order_relaxed);
      continue;
    }

    if(!m_hooks.submit || !m_hooks.submit(framePtr))
      continue;

    m_goodXfers.fetch_add(1, std::memory_order_relaxed);
  }
}

} // namespace score::gfx::interop
