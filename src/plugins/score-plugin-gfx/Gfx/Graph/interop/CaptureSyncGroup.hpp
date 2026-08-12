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
    auto& set = m_ring[gen % kDepth];

    set.generation = gen;
    set.memberCount = m_members;
    for(std::size_t i = 0; i < m_members; ++i)
    {
      set.slot[i] = slots[i];
      set.stampNs[i] = stampNs ? stampNs[i] : 0;
    }

    if(set.complete())
    {
      const auto sk = set.skewNs();
      if(sk > m_maxSkewNs.load(std::memory_order_relaxed))
        m_maxSkewNs.store(sk, std::memory_order_relaxed);
      m_newestComplete.store(gen, std::memory_order_release);
    }
    else
    {
      m_incomplete.fetch_add(1, std::memory_order_relaxed);
    }

    m_generation.store(gen, std::memory_order_release);
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
      m_pinnedIsNew = m_pinnedGen != 0 && m_pinnedGen != m_lastHandedOut;
      if(m_pinnedIsNew)
      {
        retirePinned();
        m_lastHandedOut = m_pinnedGen;
      }
    }

    if(m_pinnedGen == 0)
      return {};

    const auto& set = m_ring[m_pinnedGen % kDepth];
    // The producer may have lapped us between the pin and this read.
    if(set.generation != m_pinnedGen)
    {
      m_lapped.fetch_add(1, std::memory_order_relaxed);
      m_pinnedGen = 0;
      return {};
    }

    Latched out;
    out.slot = set.slot[member];
    out.generation = set.generation;
    out.stampNs = set.stampNs[member];
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
  /// Render thread. The capture we were holding is no longer bound, but the GPU
  /// may still be reading it: queue it, and release whatever has aged past
  /// retireDepth acquisitions. Acquisitions are counted rather than QRhi frames,
  /// and there is at most one per frame, so the count is conservative.
  void retirePinned() noexcept
  {
    ++m_acquisitions;
    if(m_lastHandedOut != 0 && m_retireN < kRetireMax)
      m_retire[m_retireN++] = Retired{m_lastHandedOut, m_acquisitions};

    std::size_t keep = 0;
    for(std::size_t i = 0; i < m_retireN; ++i)
    {
      if(m_acquisitions - m_retire[i].at < m_retireDepth)
      {
        m_retire[keep++] = m_retire[i];
        continue;
      }
      const auto& old = m_ring[m_retire[i].gen % kDepth];
      // Only if that ring entry is still the capture we retired -- otherwise the
      // producer has lapped us and those slots were already recycled.
      if(old.generation != m_retire[i].gen)
        continue;
      for(std::size_t m = 0; m < m_members; ++m)
        if(old.slot[m] >= 0 && old.slot[m] < 32)
          m_returns[m].fetch_or(
              1u << unsigned(old.slot[m]), std::memory_order_release);
    }
    m_retireN = keep;
  }

  static constexpr std::size_t kRetireMax = 8;
  struct Retired
  {
    std::uint64_t gen{};
    std::uint64_t at{};
  };

  const std::size_t m_members;

  CaptureFrameSet m_ring[kDepth]{};
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
};

}
