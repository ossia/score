#pragma once

/**
 * @file CaptureSyncGroup.hpp
 * @brief Frame-accurate correlation across several capture streams.
 *
 * A multi-sensor rig (a 360 pair, a multi-channel SDI card) produces one frame
 * per sensor per capture. The renderer must bind frames that belong to the SAME
 * capture, or a stitch shows one eye a frame ahead of the other.
 *
 * The default capture path cannot promise that. Each capture node owns a private
 * ring and latches "whatever was published last", independently: if stream A
 * published this tick and stream B did not, A binds a new frame and B silently
 * re-uses the texture it bound last tick. Nothing reports it.
 *
 * This closes both halves of that:
 *
 *   producer side -- publish() takes the whole set at once. There is no state in
 *       which half a capture is visible, so "A published, B did not" stops being
 *       representable rather than being detected after the fact.
 *
 *   consumer side -- take() pins one generation per render pass. Members are
 *       updated sequentially inside RenderList::render(), so without a pin a set
 *       published between two members' updates would split them across
 *       generations. The window is microseconds, which is exactly how a
 *       one-frame skew ships: rare, unreproducible, and only visible in the
 *       stitch.
 *
 * Only COMPLETE sets are handed out. A capture where a sensor dropped its frame
 * holds the previous complete set rather than compositing a mismatch, and counts
 * it -- a rig that is quietly tearing should show up as a number, not as an
 * artefact someone notices in a recording.
 *
 * Threading: publish() is called from one producer thread. take() is called from
 * the render thread only, so the pin is plain data; only the published ring
 * crosses threads.
 *
 * This is deliberately vendor-neutral: it knows nothing about Argus, and serves
 * any backend that can hand over N slots belonging to one capture.
 */

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace score::gfx::interop
{

/// One capture: the slot each member filled, and when the frame was exposed.
struct CaptureFrameSet
{
  static constexpr std::size_t kMaxMembers = 8;

  std::uint64_t generation{0};
  std::size_t memberCount{0};
  /// Slot index per member; < 0 means that member had no frame for this capture.
  int slot[kMaxMembers]{};
  /// Producer timestamp per member, nanoseconds, in whatever clock the producer
  /// documents. Carried through so the render side can report actual skew rather
  /// than assume the hardware delivered on its promise.
  std::uint64_t stampNs[kMaxMembers]{};

  bool complete() const noexcept
  {
    for(std::size_t i = 0; i < memberCount; ++i)
      if(slot[i] < 0)
        return false;
    return memberCount > 0;
  }

  /// Spread between the earliest and latest member stamp, in nanoseconds.
  /// Zero when the producer supplied no timestamps.
  std::uint64_t skewNs() const noexcept
  {
    std::uint64_t lo = ~std::uint64_t(0), hi = 0;
    for(std::size_t i = 0; i < memberCount; ++i)
    {
      const auto s = stampNs[i];
      if(s == 0)
        continue;
      if(s < lo)
        lo = s;
      if(s > hi)
        hi = s;
    }
    return hi >= lo ? hi - lo : 0;
  }
};

/// A ring entry: the payload plus the version that guards it. `seq` is 0 while
/// the producer is rewriting the entry and equals the generation once the
/// payload is whole, so a reader can tell a complete set from one it caught
/// mid-write.
struct CaptureRingEntry
{
  std::atomic<std::uint64_t> seq{0};
  CaptureFrameSet set{};
};

class CaptureSyncGroup
{
public:
  /// Depth of the published history. A pinned set must survive while the
  /// producer keeps publishing, so this bounds how far the render thread may
  /// lag before the set it pinned is overwritten underneath it.
  static constexpr std::size_t kDepth = 4;

  explicit CaptureSyncGroup(std::size_t memberCount) noexcept
      : m_members{
            memberCount < CaptureFrameSet::kMaxMembers
                ? memberCount
                : CaptureFrameSet::kMaxMembers}
  {
  }

  std::size_t memberCount() const noexcept { return m_members; }

  /// Producer. `slots` and `stampNs` are memberCount long; a slot < 0 marks a
  /// member that had no frame for this capture. `stampNs` may be null.
  void publish(const int* slots, const std::uint64_t* stampNs) noexcept
  {
    const auto gen = m_generation.load(std::memory_order_relaxed) + 1;
    auto& e = m_ring[gen % kDepth];

    // Retire the version before touching the payload: a reader that catches the
    // entry mid-write then sees a version it cannot match and retries, instead
    // of assembling one member from this capture and another from the last.
    e.seq.store(0, std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_release);

    e.set.generation = gen;
    e.set.memberCount = m_members;
    for(std::size_t i = 0; i < m_members; ++i)
    {
      // takeReturned() hands slots back in a 32-bit mask, so a slot it cannot
      // name is a slot the producer would never get back. Drop the capture
      // rather than lend a buffer that can only leak.
      e.set.slot[i] = slots[i] < int(kMaxReturnableSlot) ? slots[i] : -1;
      e.set.stampNs[i] = stampNs ? stampNs[i] : 0;
    }

    e.seq.store(gen, std::memory_order_release);

    const bool complete = e.set.complete();
    if(complete)
    {
      const auto sk = e.set.skewNs();
      if(sk > m_maxSkewNs.load(std::memory_order_relaxed))
        m_maxSkewNs.store(sk, std::memory_order_relaxed);
    }
    else
    {
      m_incomplete.fetch_add(1, std::memory_order_relaxed);
    }

    // m_generation first: take() derives the lap distance as
    // m_generation - m_newestComplete, so publishing the complete marker ahead
    // of the generation lets that subtraction wrap and condemn a fresh set.
    m_generation.store(gen, std::memory_order_release);
    if(complete)
      m_newestComplete.store(gen, std::memory_order_release);
  }

  struct Latched
  {
    int slot{-1};
    std::uint64_t generation{0};
    std::uint64_t stampNs{0};
    /// True when this pass pinned a capture the group has not handed out before.
    /// Group-level on purpose: each member binds its own texture, so a new
    /// capture must be reported to every member or the others stay bound to the
    /// previous one.
    bool fresh{false};
  };

  /// Render thread. Every member asking with the same `passId` is answered from
  /// the same capture. `passId` is RenderList::frame.
  Latched take(std::size_t member, std::int64_t passId) noexcept
  {
    if(member >= m_members)
      return {};

    if(passId != m_pinnedPass)
    {
      m_pinnedPass = passId;
      const auto gen = m_newestComplete.load(std::memory_order_acquire);
      // A set older than the ring depth has been overwritten by the producer
      // while the render thread was behind; there is nothing coherent left to
      // bind, so hold rather than read a torn set.
      const auto newest = m_generation.load(std::memory_order_acquire);
      if(gen == 0 || newest - gen >= kDepth)
      {
        if(gen != 0)
          m_lapped.fetch_add(1, std::memory_order_relaxed);
        m_pinnedGen = 0;
      }
      else
      {
        m_pinnedGen = gen;
      }
      // Copy the whole capture out of the ring here, once. Re-reading it per
      // member would let a lap between two members answer one from the new
      // capture and the other from the old -- the split this class exists to
      // make unrepresentable.
      if(m_pinnedGen != 0 && !snapshotPinned())
      {
        m_lapped.fetch_add(1, std::memory_order_relaxed);
        m_pinnedGen = 0;
      }

      m_pinnedIsNew = m_pinnedGen != 0 && m_pinnedGen != m_lastHandedOut;
      if(m_pinnedIsNew)
      {
        queueRetire();
        m_lastHandedOut = m_pinnedGen;
        for(std::size_t i = 0; i < m_members; ++i)
          m_handedOutSlots[i] = m_pinnedSet.slot[i];
      }
      // Age the queue on every pass. Tying this to a fresh capture deadlocks:
      // ageing needs a new capture, a new capture needs a free slot, and slots
      // only come free by ageing. A producer whose ring is no deeper than the
      // retire depth never escapes that loop.
      ageRetired();
    }

    if(m_pinnedGen == 0)
      return {};

    Latched out;
    out.slot = m_pinnedSet.slot[member];
    out.generation = m_pinnedSet.generation;
    out.stampNs = m_pinnedSet.stampNs[member];
    out.fresh = m_pinnedIsNew;
    return out;
  }

  /// `framesInFlight + 1`, from QRhi::resourceLimit(QRhi::FramesInFlight).
  /// Set before the first publish.
  void setRetireDepth(std::size_t d) noexcept { m_retireDepth = d ? d : 1; }

  /// Producer thread. Bitmask of `member`'s slots the renderer has finished
  /// with, which may now go back to the device.
  ///
  /// Slot lifetime lives here rather than in each strategy's BorrowedSlotTracker
  /// because with a group the renderer no longer consumes the strategy's own
  /// publisher -- leaving both to arbitrate would let a strategy hand back a slot
  /// the group had just pinned.
  std::uint32_t takeReturned(std::size_t member) noexcept
  {
    if(member >= m_members)
      return 0;
    return m_returns[member].exchange(0, std::memory_order_acquire);
  }

  /// Captures dropped because at least one member had no frame.
  std::uint64_t incompleteCount() const noexcept
  {
    return m_incomplete.load(std::memory_order_relaxed);
  }
  /// Times the render thread fell far enough behind that its pinned set was
  /// overwritten. Non-zero means the pipeline is not keeping up, not that the
  /// sensors are out of sync.
  std::uint64_t lappedCount() const noexcept
  {
    return m_lapped.load(std::memory_order_relaxed);
  }
  /// Worst intra-capture spread seen, nanoseconds. This is the number that says
  /// whether the hardware sync is actually working.
  std::uint64_t maxSkewNs() const noexcept
  {
    return m_maxSkewNs.load(std::memory_order_relaxed);
  }

private:
  /// takeReturned()'s mask width. A slot index at or above this cannot be
  /// handed back, so publish() refuses it.
  static constexpr std::size_t kMaxReturnableSlot = 32;

  static constexpr std::size_t kRetireMax = 8;
  struct Retired
  {
    std::uint64_t gen{};
    std::uint64_t at{};
    int slot[CaptureFrameSet::kMaxMembers]{};
  };

  /// Render thread. Take a coherent copy of the pinned ring entry, or fail if
  /// the producer overwrote it while we were reading.
  bool snapshotPinned() noexcept
  {
    const auto& e = m_ring[m_pinnedGen % kDepth];
    if(e.seq.load(std::memory_order_acquire) != m_pinnedGen)
      return false;
    m_pinnedSet = e.set;
    std::atomic_thread_fence(std::memory_order_acquire);
    return e.seq.load(std::memory_order_acquire) == m_pinnedGen;
  }

  /// Render thread. The capture we were holding is no longer bound, but the GPU
  /// may still be reading it: queue it, and release whatever has aged past
  /// retireDepth acquisitions. Acquisitions are counted rather than QRhi frames,
  /// and there is at most one per frame, so the count is conservative.
  void queueRetire() noexcept
  {
    if(m_lastHandedOut == 0)
      return;
    if(m_retireN == kRetireMax)
    {
      // Full: release the oldest rather than drop it. Dropping strands its
      // slots with the renderer forever; the oldest is also the one most
      // likely to be past the GPU already.
      releaseSlotsOf(m_retire[0]);
      for(std::size_t i = 1; i < m_retireN; ++i)
        m_retire[i - 1] = m_retire[i];
      --m_retireN;
    }
    auto& r = m_retire[m_retireN++];
    r.gen = m_lastHandedOut;
    r.at = m_acquisitions + 1;
    for(std::size_t m = 0; m < m_members; ++m)
      r.slot[m] = m_handedOutSlots[m];
  }

  void ageRetired() noexcept
  {
    ++m_acquisitions;
    std::size_t keep = 0;
    for(std::size_t i = 0; i < m_retireN; ++i)
    {
      if(m_acquisitions - m_retire[i].at < m_retireDepth)
      {
        m_retire[keep++] = m_retire[i];
        continue;
      }
      releaseSlotsOf(m_retire[i]);
    }
    m_retireN = keep;
  }

  void releaseSlotsOf(const Retired& r) noexcept
  {
    // The slots come from the record, not from the ring: a lapped entry no
    // longer describes the capture we handed out, and its slots are still on
    // loan to the renderer until we return them here.
    for(std::size_t m = 0; m < m_members; ++m)
      if(r.slot[m] >= 0 && r.slot[m] < int(kMaxReturnableSlot))
        m_returns[m].fetch_or(1u << unsigned(r.slot[m]), std::memory_order_release);
  }


  const std::size_t m_members;

  CaptureRingEntry m_ring[kDepth]{};
  std::atomic<std::uint64_t> m_generation{0};
  std::atomic<std::uint64_t> m_newestComplete{0};

  std::atomic<std::uint64_t> m_incomplete{0};
  std::atomic<std::uint64_t> m_lapped{0};
  std::atomic<std::uint64_t> m_maxSkewNs{0};
  std::atomic<std::uint32_t> m_returns[CaptureFrameSet::kMaxMembers]{};

  std::size_t m_retireDepth{1};
  std::uint64_t m_acquisitions{0};
  Retired m_retire[kRetireMax]{};
  std::size_t m_retireN{0};

  // Render-thread only.
  std::int64_t m_pinnedPass{-1};
  std::uint64_t m_pinnedGen{0};
  std::uint64_t m_lastHandedOut{0};
  bool m_pinnedIsNew{false};
  CaptureFrameSet m_pinnedSet{};
  int m_handedOutSlots[CaptureFrameSet::kMaxMembers]{};
};

}
