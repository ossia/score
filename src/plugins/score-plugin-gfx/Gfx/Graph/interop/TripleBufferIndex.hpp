#pragma once

/**
 * @file TripleBufferIndex.hpp
 * @brief Lock-free slot rotation behind TextureShare's producer/consumer pair.
 *
 * Three slots: 0, 1, 2
 * - Producer writes to 'write' slot
 * - When done, producer swaps 'write' with 'ready'
 * - Consumer swaps 'ready' with 'read' when it wants new data
 * - Consumer reads from 'read' slot
 *
 * This ensures producer and consumer never access the same slot simultaneously.
 */

#include <atomic>
#include <cstdint>

namespace score::gfx
{

class TripleBufferIndex
{
public:
  TripleBufferIndex()
  {
    m_state.store(makeState(0, 1, 2, false), std::memory_order_relaxed);
  }

  // Called by producer when starting a new frame
  int acquireWriteIndex() noexcept
  {
    uint32_t state = m_state.load(std::memory_order_acquire);
    return writeIndex(state);
  }

  // Called by producer when frame is complete - swaps write and ready
  void publishWriteIndex() noexcept
  {
    uint32_t oldState, newState;
    do
    {
      oldState = m_state.load(std::memory_order_acquire);
      int w = writeIndex(oldState);
      int r = readyIndex(oldState);
      int rd = readIndex(oldState);
      newState = makeState(r, w, rd, true);
    } while(!m_state.compare_exchange_weak(
        oldState, newState, std::memory_order_release, std::memory_order_relaxed));
  }

  // Called by consumer to get latest frame - swaps ready and read if new frame available
  // Returns the read index, or -1 if no new frame
  int acquireReadIndex() noexcept
  {
    uint32_t oldState, newState;
    do
    {
      oldState = m_state.load(std::memory_order_acquire);
      if(!hasNewFrame(oldState))
        return readIndex(oldState);

      int w = writeIndex(oldState);
      int r = readyIndex(oldState);
      int rd = readIndex(oldState);
      newState = makeState(w, rd, r, false);
    } while(!m_state.compare_exchange_weak(
        oldState, newState, std::memory_order_release, std::memory_order_relaxed));

    return readyIndex(oldState); // Return the old ready (now read) index
  }

  bool hasNewFrameAvailable() const noexcept
  {
    return hasNewFrame(m_state.load(std::memory_order_acquire));
  }

private:
  // State packing: [write:2][ready:2][read:2][hasNew:1] in lowest 7 bits
  static constexpr uint32_t makeState(int w, int r, int rd, bool hasNew) noexcept
  {
    return (uint32_t(w) << 5) | (uint32_t(r) << 3) | (uint32_t(rd) << 1)
           | (hasNew ? 1 : 0);
  }
  static constexpr int writeIndex(uint32_t state) noexcept { return (state >> 5) & 3; }
  static constexpr int readyIndex(uint32_t state) noexcept { return (state >> 3) & 3; }
  static constexpr int readIndex(uint32_t state) noexcept { return (state >> 1) & 3; }
  static constexpr bool hasNewFrame(uint32_t state) noexcept { return state & 1; }

  std::atomic<uint32_t> m_state;
};

} // namespace score::gfx
