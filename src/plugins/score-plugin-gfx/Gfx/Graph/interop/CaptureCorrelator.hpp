#pragma once

/**
 * @file CaptureCorrelator.hpp
 * @brief Assembles one CaptureFrameSet out of several independent capture
 *        threads, for a rig whose sensors are separate V4L2 devices.
 *
 * CaptureSyncGroup::publish() writes every member of a set and bumps the
 * generation in one call, which suits a device that hands over all its sensors
 * from a single capture -- Argus' multi-sensor session, an SDI card with several
 * inputs. Two USB or CSI cameras are not that: each has its own file
 * descriptor, its own thread and its own arrival time, and two threads calling
 * publish() would interleave halves of different captures into one set.
 *
 * This is the piece in front of the group that turns N independent arrivals
 * into one publication. Each thread offers its own member and nothing else;
 * the offer that completes the row publishes it.
 *
 * Pairing is by arrival, not by timestamp. On a frequency-locked rig -- sensors
 * sharing a clock, which is the case this exists for -- consecutive arrivals
 * correspond, and the eyes sit a constant offset apart that no amount of
 * matching will remove. Timestamps are carried through so the group can report
 * the skew that actually occurred rather than the skew the hardware promised.
 *
 * A member that outruns its partners displaces its own previous offer: the
 * older frame can no longer belong to any set, and its slot is handed straight
 * back to the caller for requeueing. Holding it instead would starve the
 * driver of buffers, which presents as a stall rather than as a drop.
 *
 * Only complete rows are published. A member that stops delivering therefore
 * holds the whole rig, which is deliberate twice over: it is what a rig means,
 * and CaptureSyncGroup::take() serves only the newest *complete* set anyway.
 * Publishing partial rows would not keep the live members going -- it would
 * advance the group's generation while the newest complete one stood still,
 * and once that gap reaches the ring depth the last good set is treated as
 * lapped and every member goes dark. `displacedFrames()` climbing at the frame
 * rate is what the stall looks like from here.
 */

#include <Gfx/Graph/interop/CaptureSyncGroup.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>

namespace score::gfx::interop
{

class CaptureCorrelator
{
public:
  explicit CaptureCorrelator(std::size_t memberCount) noexcept
      : m_group{memberCount}
  {
  }

  CaptureSyncGroup& group() noexcept { return m_group; }
  std::size_t memberCount() const noexcept { return m_group.memberCount(); }

  /**
   * @brief Offer this member's freshly captured slot.
   *
   * Called from the owning member's capture thread. Returns the slot the
   * caller must give back to its driver -- its own previous offer, when this
   * one displaced it -- or -1 when nothing has to be returned.
   */
  int offer(std::size_t member, int slot, std::uint64_t stampNs) noexcept
  {
    if(member >= memberCount() || slot < 0)
      return -1;

    std::lock_guard lock{m_mutex};

    int displaced = -1;
    if(m_pending[member].slot >= 0)
    {
      displaced = m_pending[member].slot;
      m_displaced.fetch_add(1, std::memory_order_relaxed);
    }
    m_pending[member] = Pending{slot, stampNs};

    for(std::size_t i = 0; i < memberCount(); ++i)
      if(m_pending[i].slot < 0)
        return displaced;

    publishLocked();
    return displaced;
  }

  /**
   * @brief Give up every offer being held, for teardown or a format change.
   *
   * Writes the held slots into `out` (memberCount long, -1 where nothing was
   * held) so each owner can requeue its own. The group is left alone: the
   * renderer may still be reading a set it pinned.
   */
  void drain(int* out) noexcept
  {
    std::lock_guard lock{m_mutex};
    for(std::size_t i = 0; i < memberCount(); ++i)
    {
      out[i] = m_pending[i].slot;
      m_pending[i] = Pending{};
    }
  }

  std::uint64_t setsPublished() const noexcept
  {
    return m_sets.load(std::memory_order_relaxed);
  }
  /// Frames dropped because their member outran the rest of the rig. A member
  /// whose partner has stopped delivering displaces on every capture, so this
  /// climbing at the frame rate is what a stalled rig looks like from here.
  std::uint64_t displacedFrames() const noexcept
  {
    return m_displaced.load(std::memory_order_relaxed);
  }

private:
  struct Pending
  {
    int slot{-1};
    std::uint64_t stampNs{0};
  };

  void publishLocked() noexcept
  {
    int slots[CaptureFrameSet::kMaxMembers];
    std::uint64_t stamps[CaptureFrameSet::kMaxMembers];
    for(std::size_t i = 0; i < memberCount(); ++i)
    {
      slots[i] = m_pending[i].slot;
      stamps[i] = m_pending[i].stampNs;
      m_pending[i] = Pending{};
    }
    m_group.publish(slots, stamps);
    m_sets.fetch_add(1, std::memory_order_relaxed);
  }

  CaptureSyncGroup m_group;

  std::mutex m_mutex;
  Pending m_pending[CaptureFrameSet::kMaxMembers]{};

  std::atomic<std::uint64_t> m_sets{0};
  std::atomic<std::uint64_t> m_displaced{0};
};

} // namespace score::gfx::interop
