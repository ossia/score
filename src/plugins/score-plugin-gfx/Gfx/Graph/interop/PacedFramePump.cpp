#include <Gfx/Graph/interop/PacedFramePump.hpp>

#include <chrono>

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
  // Wake the consumer if it is parked on the frames-available semaphore so
  // the join is immediate rather than a timeout away.
  m_framesAvail.release();
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
  m_framesAvail.release();
  return true;
}

void PacedFramePump::run()
{
  using namespace std::chrono_literals;
  while(m_running.load(std::memory_order_relaxed))
  {
    // 1. Park until the producer has push()ed something (or timeout, to
    //    re-check shutdown). Waiting on frames FIRST — not on the vendor tick —
    //    guarantees waitForTick() is only invoked when a submit will follow,
    //    and that an idle pump sleeps instead of spinning on free-running tick
    //    hooks.
    if(!m_framesAvail.try_acquire_for(100ms))
      continue;

    if(!m_running.load(std::memory_order_relaxed))
      break;

    // Spurious/stale permit (e.g. released by stop(), or a frame we already
    // drained ahead below)? Nothing pending — don't consume a tick.
    if(m_writeIdx.load(std::memory_order_acquire)
       == m_readIdx.load(std::memory_order_relaxed))
      continue;

    // 2. Pacing. Two vendor shapes:
    //    - canAccept provided (AJA AutoCirculate): the card paces playout
    //      from its own frame ring — stuff that ring back-to-back whenever
    //      it can accept, and block on the vendor tick ONLY when it can't.
    //      Ticking before *every* submit (the previous behaviour)
    //      serialises tick-period + DMA and caps throughput at
    //      1/(VBI + transfer): ~46 fps at UHD60, ~24 fps at 8K.
    //    - canAccept null (DeckLink free-pool semaphore, Bluefish output
    //      sync, Deltacast blocking submit): waitForTick IS the
    //      back-pressure — keep the one-tick-per-submit contract.
    if(m_hooks.canAccept)
    {
      while(m_running.load(std::memory_order_relaxed) && !m_hooks.canAccept())
      {
        if(m_hooks.waitForTick)
          (void)m_hooks.waitForTick(); // timeout => re-check shutdown
        else
          break;
      }
      if(!m_running.load(std::memory_order_relaxed))
        break;
    }
    else
    {
      if(!m_hooks.waitForTick || !m_hooks.waitForTick())
      {
        m_framesAvail.release();
        continue;
      }
    }

    if(!m_running.load(std::memory_order_relaxed))
      break;

    // 3. Drain the ring of any slots produced since the last tick - we submit
    //    the most recent one and drop earlier (older) ones rather than fall
    //    behind the output clock. (For lipsync-strict cases this would be
    //    one-per-tick; first cut prefers freshness.)
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

    // We consumed one permit up front; consume the permits of the extra
    // frames we just drained past so the semaphore keeps tracking the ring.
    for(uint32_t skipped = (consume - r); skipped > 1; --skipped)
      (void)m_framesAvail.try_acquire();

    if(!framePtr)
      continue;

    // Re-check back-pressure right before submit (the gate above ran before
    // the drain; a canAccept flip in between is unlikely but cheap to guard).
    // Unlike before, do NOT discard the frame — wait the tick out.
    while(m_running.load(std::memory_order_relaxed) && m_hooks.canAccept
          && !m_hooks.canAccept())
    {
      m_drops.fetch_add(1, std::memory_order_relaxed);
      if(m_hooks.waitForTick)
        (void)m_hooks.waitForTick();
      else
        break;
    }
    if(!m_running.load(std::memory_order_relaxed))
      break;

    if(!m_hooks.submit || !m_hooks.submit(framePtr))
      continue;

    m_goodXfers.fetch_add(1, std::memory_order_relaxed);
  }
}

} // namespace score::gfx::interop
