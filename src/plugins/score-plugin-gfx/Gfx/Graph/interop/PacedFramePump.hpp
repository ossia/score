#pragma once

/**
 * @file PacedFramePump.hpp
 * @brief Vendor-neutral VBI/clock-paced frame submission.
 *
 * Decouples score's render() (producer) from a card's output clock (consumer).
 * Every capture-card output addon needs the same machinery: a small lock-free
 * SPSC ring the render thread pushes frame pointers into, and a consumer thread
 * that — on each output tick (SDI VBI / ScheduleFrame callback / sync) — pops
 * the *most recent* frame and submits it to the card. If the producer outruns
 * the clock the ring overflows and the oldest in-flight frame is dropped; if it
 * falls behind, an underrun is counted and the card replays its previous frame.
 *
 * This is the generalisation of AJA's AJAConsumerThread. The vendor supplies
 * three hooks; everything else (ring, thread lifecycle, drain-to-newest,
 * drop/underrun accounting) lives here once:
 *
 *   - `waitForTick()`  block until the next output tick; return false on a
 *                      timeout so the loop can re-check shutdown (it just loops).
 *   - `canAccept()`    optional card-side back-pressure probe; nullptr => always
 *                      accept. Returning false drops the frame (counted).
 *   - `submit(ptr)`    DMA/schedule the frame at `ptr`; return false on failure.
 *
 * Pacing order: the pump first waits until at least one frame has been
 * push()ed, and only THEN calls waitForTick(). Consequences vendors can rely
 * on: (a) waitForTick() is never invoked with an empty ring, so a hook that
 * consumes a scarce resource per call (e.g. a free-output-slot permit) never
 * loses it to an idle tick; (b) an always-true waitForTick() does not
 * busy-spin — the pump blocks on frame arrival, not on the hook; (c) if
 * waitForTick() returns false (timeout), the pending frame stays queued and
 * the tick is retried.
 *
 * The producer must ensure all GPU work filling `ptr` is complete before push()
 * (the pump does no cross-backend sync — strategies sync inside prepareNextFrame).
 *
 * `ptr` is opaque: a GPU device pointer (GPU-direct path) or a pinned host
 * pointer (host-staged path) — the pump never dereferences it.
 */

#include <score_plugin_gfx_export.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <semaphore>
#include <thread>
#include <vector>

namespace score::gfx::interop
{

class SCORE_PLUGIN_GFX_EXPORT PacedFramePump
{
public:
  struct Hooks
  {
    /// Block until the next output tick. Return false on timeout (the pending
    /// frame stays queued and the tick is retried). Only ever called when at
    /// least one frame is pending — see the pacing-order note above. Should
    /// return periodically so stop() stays responsive while frames are
    /// pending.
    std::function<bool()> waitForTick;
    /// Optional card-side back-pressure. nullptr => always accept. Returning
    /// false drops the current frame (counted as a drop).
    std::function<bool()> canAccept;
    /// Submit/DMA the frame at `framePtr`. Return false on failure.
    std::function<bool(void* framePtr)> submit;
  };

  /// `ringDepth` is the producer/consumer decoupling depth (AJA used 3).
  explicit PacedFramePump(Hooks hooks, int ringDepth = 3);
  ~PacedFramePump();

  PacedFramePump(const PacedFramePump&) = delete;
  PacedFramePump& operator=(const PacedFramePump&) = delete;

  /// Spin up the consumer thread. Idempotent.
  void start();
  /// Signal stop and join. Idempotent.
  void stop();

  /// Producer-side: enqueue a frame pointer for the next tick. Non-blocking;
  /// returns false (and counts a drop) when the ring is full.
  bool push(void* framePtr);

  uint64_t goodXfers() const { return m_goodXfers.load(std::memory_order_relaxed); }
  uint64_t drops() const { return m_drops.load(std::memory_order_relaxed); }
  uint64_t underruns() const { return m_underruns.load(std::memory_order_relaxed); }

private:
  void run();

  Hooks m_hooks;
  const int m_depth;

  // Slot stores a frame pointer. nullptr = empty. A non-null write followed by
  // a writeIdx release-bump publishes it; the consumer reads with acquire.
  std::vector<std::atomic<void*>> m_slots;
  std::atomic<uint32_t> m_writeIdx{0};
  std::atomic<uint32_t> m_readIdx{0};

  // One permit per successfully push()ed frame; the consumer blocks here (not
  // in waitForTick) while idle. Drained-ahead frames return their permits via
  // try_acquire so the count tracks the ring.
  std::counting_semaphore<> m_framesAvail{0};

  std::thread m_thread;
  std::atomic<bool> m_running{false};

  std::atomic<uint64_t> m_goodXfers{0};
  std::atomic<uint64_t> m_drops{0};
  std::atomic<uint64_t> m_underruns{0};
};

} // namespace score::gfx::interop
