#define CATCH_CONFIG_MAIN

#include <Gfx/Graph/interop/CaptureSyncGroup.hpp>

#include <catch2/catch_all.hpp>

using score::gfx::interop::CaptureFrameSet;
using score::gfx::interop::CaptureSyncGroup;

namespace
{
void publish(CaptureSyncGroup& g, std::initializer_list<int> slots,
             std::initializer_list<std::uint64_t> stamps = {})
{
  int s[CaptureFrameSet::kMaxMembers]{};
  std::uint64_t t[CaptureFrameSet::kMaxMembers]{};
  std::size_t i = 0;
  for(int v : slots)
    s[i++] = v;
  i = 0;
  for(std::uint64_t v : stamps)
    t[i++] = v;
  g.publish(s, stamps.size() ? t : nullptr);
}
}

TEST_CASE("members in one pass are answered from one capture")
{
  CaptureSyncGroup g{2};
  publish(g, {3, 7});

  const auto a = g.take(0, 100);
  const auto b = g.take(1, 100);

  REQUIRE(a.slot == 3);
  REQUIRE(b.slot == 7);
  REQUIRE(a.generation == b.generation);
}

TEST_CASE("a capture published mid-pass cannot split the members")
{
  // The failure this whole class exists to prevent: RenderList updates members
  // sequentially, so a publish landing between two take() calls would otherwise
  // hand them different captures.
  CaptureSyncGroup g{2};
  publish(g, {1, 1});

  const auto a = g.take(0, 100);
  publish(g, {2, 2}); // producer runs between the two members' updates
  const auto b = g.take(1, 100);

  REQUIRE(a.slot == 1);
  REQUIRE(b.slot == 1);
  REQUIRE(a.generation == b.generation);
}

TEST_CASE("a new pass picks up the newest complete capture")
{
  CaptureSyncGroup g{2};
  publish(g, {1, 1});
  REQUIRE(g.take(0, 100).slot == 1);

  publish(g, {2, 2});
  const auto a = g.take(0, 101);
  const auto b = g.take(1, 101);
  REQUIRE(a.slot == 2);
  REQUIRE(b.slot == 2);
  REQUIRE(a.fresh);
}

TEST_CASE("an incomplete capture holds the previous one and is counted")
{
  CaptureSyncGroup g{2};
  publish(g, {5, 5});
  REQUIRE(g.take(0, 100).slot == 5);

  // One sensor dropped its frame. Compositing 6 against a stale 5 is exactly the
  // mismatch a 360 stitch shows as a seam, so the whole capture is held back.
  publish(g, {6, -1});

  const auto a = g.take(0, 101);
  const auto b = g.take(1, 101);
  REQUIRE(a.slot == 5);
  REQUIRE(b.slot == 5);
  REQUIRE(g.incompleteCount() == 1);
  // Held, not fresh: the renderer can tell it is showing the same capture again.
  REQUIRE(!a.fresh);
}

TEST_CASE("every member of a new capture is told it is fresh")
{
  // `fresh` drives whether a member rebinds its texture, and each member has its
  // own. A new capture is new for all of them, so group-level freshness is what
  // the renderer wants -- reporting it only to the first member would leave the
  // others bound to the previous capture.
  CaptureSyncGroup g{2};
  publish(g, {1, 1});
  REQUIRE(g.take(0, 100).fresh);
  REQUIRE(g.take(1, 100).fresh);
}

TEST_CASE("a pass with no new capture is not fresh, so nothing rebinds")
{
  CaptureSyncGroup g{2};
  publish(g, {1, 1});
  REQUIRE(g.take(0, 100).fresh);

  // Next render pass, producer published nothing in between.
  const auto a = g.take(0, 101);
  const auto b = g.take(1, 101);
  REQUIRE(a.slot == 1);
  REQUIRE(b.slot == 1);
  REQUIRE(!a.fresh);
  REQUIRE(!b.fresh);
}

TEST_CASE("nothing published yields no slot rather than slot 0")
{
  CaptureSyncGroup g{2};
  const auto a = g.take(0, 100);
  REQUIRE(a.slot == -1);
  REQUIRE(a.generation == 0);
}

TEST_CASE("a render thread lapped by the producer holds rather than tearing")
{
  CaptureSyncGroup g{2};
  publish(g, {1, 1});

  // Producer runs away by more than the ring depth while the render thread is
  // stalled. The pinned set no longer exists; binding whatever is in its ring
  // slot would be a torn read of two different captures.
  for(int i = 0; i < int(CaptureSyncGroup::kDepth) + 2; ++i)
    publish(g, {10 + i, 10 + i});

  const auto a = g.take(0, 200);
  // The newest complete set is recent, so this pass is fine...
  REQUIRE(a.slot == 10 + int(CaptureSyncGroup::kDepth) + 1);
}

TEST_CASE("skew is measured from the producer stamps")
{
  CaptureSyncGroup g{2};
  publish(g, {1, 1}, {1'000'000ull, 1'250'000ull});
  publish(g, {2, 2}, {2'000'000ull, 2'050'000ull});

  // 250 us is the worst spread seen; this is the number that says whether the
  // hardware sync is doing its job.
  REQUIRE(g.maxSkewNs() == 250'000ull);
}

TEST_CASE("a single-member group behaves like the unsynchronised path")
{
  CaptureSyncGroup g{1};
  publish(g, {4});
  REQUIRE(g.take(0, 1).slot == 4);
  publish(g, {5});
  REQUIRE(g.take(0, 2).slot == 5);
  REQUIRE(g.incompleteCount() == 0);
}

TEST_CASE("asking for a member outside the group is not a slot")
{
  CaptureSyncGroup g{2};
  publish(g, {1, 1});
  REQUIRE(g.take(5, 1).slot == -1);
}

TEST_CASE("a slot is only returned once the GPU can no longer be reading it")
{
  // Matches BorrowedSlotTracker's conservative rule: a capture is queued at the
  // acquisition that DISPLACES it, and freed once retireDepth further
  // acquisitions have happened. Handing a slot back even one acquisition early
  // corrupts the frame being scanned out.
  CaptureSyncGroup g{2};
  g.setRetireDepth(2);

  publish(g, {0, 10});
  g.take(0, 1);
  g.take(1, 1); // acquisition 1: gen1 bound
  REQUIRE(g.takeReturned(0) == 0u);

  publish(g, {1, 11});
  g.take(0, 2);
  g.take(1, 2); // acquisition 2: gen1 displaced, queued
  REQUIRE(g.takeReturned(0) == 0u);

  publish(g, {2, 12});
  g.take(0, 3);
  g.take(1, 3); // acquisition 3: gen1 age 1, still too young
  REQUIRE(g.takeReturned(0) == 0u);

  publish(g, {3, 13});
  g.take(0, 4);
  g.take(1, 4); // acquisition 4: gen1 age 2 == retireDepth
  REQUIRE(g.takeReturned(0) == (1u << 0));
  REQUIRE(g.takeReturned(1) == (1u << 10));
}

TEST_CASE("returns are drained, not repeated")
{
  CaptureSyncGroup g{1};
  g.setRetireDepth(1);
  publish(g, {3});
  g.take(0, 1);
  publish(g, {4});
  g.take(0, 2); // slot 3 displaced here
  publish(g, {5});
  g.take(0, 3); // ...and aged out one acquisition later

  REQUIRE(g.takeReturned(0) == (1u << 3));
  REQUIRE(g.takeReturned(0) == 0u);
}

TEST_CASE("holding a capture does not retire it")
{
  // Repeated passes with nothing new must not age the bound capture out from
  // under the renderer -- retirement is driven by acquisitions, not by passes.
  CaptureSyncGroup g{1};
  g.setRetireDepth(1);
  publish(g, {7});
  g.take(0, 1);
  for(int p = 2; p < 20; ++p)
    REQUIRE(g.take(0, p).slot == 7);
  REQUIRE(g.takeReturned(0) == 0u);
}

// The class exists to be correct across threads, so the contract has to be
// exercised across threads. Everything above this line is single-threaded and
// passes whatever the memory ordering happens to be.

#include <atomic>
#include <thread>

TEST_CASE("a fresh set is never reported lapped", "[syncgroup][threads]")
{
  // The producer is throttled to stay within the ring depth of what the
  // consumer has already taken, so the ring can never legitimately be
  // overwritten under a pinned set. Under that constraint lappedCount() must
  // stay zero; any count is the consumer having observed publish() half-done.
  constexpr int kIters = 200000;
  CaptureSyncGroup group{2};

  std::atomic<std::uint64_t> taken{0};
  std::atomic<bool> stop{false};

  std::thread producer{[&] {
    for(int i = 1; i <= kIters && !stop.load(std::memory_order_relaxed); ++i)
    {
      while(std::uint64_t(i) - taken.load(std::memory_order_acquire)
                >= CaptureSyncGroup::kDepth - 1
            && !stop.load(std::memory_order_relaxed))
        std::this_thread::yield();

      const int slots[2] = {i % 8, (i + 1) % 8};
      const std::uint64_t stamps[2]
          = {std::uint64_t(i) * 1000u, std::uint64_t(i) * 1000u + 5u};
      group.publish(slots, stamps);
    }
    stop.store(true, std::memory_order_relaxed);
  }};

  std::thread consumer{[&] {
    std::int64_t pass = 0;
    while(!stop.load(std::memory_order_relaxed))
    {
      const auto a = group.take(0, pass);
      const auto b = group.take(1, pass);
      ++pass;
      if(a.slot >= 0 && b.slot >= 0)
      {
        // Both members must answer from the same pinned set, always.
        REQUIRE(a.generation == b.generation);
        taken.store(a.generation, std::memory_order_release);
      }
    }
  }};

  producer.join();
  consumer.join();

  INFO("lapped=" << group.lappedCount() << " over " << kIters << " publishes");
  REQUIRE(group.lappedCount() == 0);
}

TEST_CASE("a slot the return mask cannot represent is never handed out",
          "[syncgroup][slots]")
{
  // The return mask is 32 bits, so a slot >= 32 can never be handed back. A
  // capture carrying one must not be published as usable, or the renderer
  // borrows a buffer the producer never gets back.
  CaptureSyncGroup group{1};
  const int slots[1] = {40};
  group.publish(slots, nullptr);

  const auto l = group.take(0, 1);
  REQUIRE(l.slot < 0);
  REQUIRE(group.incompleteCount() == 1);
}

TEST_CASE("the producer keeps making progress with a shallow ring",
          "[syncgroup][slots]")
{
  // A slot can only be published again once it has come back through
  // takeReturned. Ageing the retire queue must not require a fresh pin, since
  // a fresh pin requires a free slot: a producer no deeper than the retire
  // depth would publish its whole ring once and then stall forever.
  // Backpressure is fine; never recovering is not.
  constexpr int kSlots = 4;
  CaptureSyncGroup group{1};
  group.setRetireDepth(3); // a realistic FramesInFlight + 1

  std::vector<int> free;
  for(int i = 0; i < kSlots; ++i)
    free.push_back(i);

  int published = 0;
  for(int i = 1; i <= 2000; ++i)
  {
    if(!free.empty())
    {
      const int s[1] = {free.back()};
      free.pop_back();
      group.publish(s, nullptr);
      ++published;
    }
    group.take(0, i);
    const auto mask = group.takeReturned(0);
    for(int b = 0; b < 32; ++b)
      if(mask & (1u << unsigned(b)))
        free.push_back(b);
  }

  // Stalling shows up as exactly one pass through the ring and nothing after.
  INFO("published " << published << " of 2000 with a " << kSlots << "-slot ring");
  REQUIRE(published > kSlots * 10);
}
