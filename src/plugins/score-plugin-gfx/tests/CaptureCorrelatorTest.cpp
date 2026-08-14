#define CATCH_CONFIG_MAIN

#include <Gfx/Graph/interop/CaptureCorrelator.hpp>

#include <catch2/catch_all.hpp>

#include <atomic>
#include <thread>
#include <vector>

using score::gfx::interop::CaptureCorrelator;
using score::gfx::interop::CaptureFrameSet;

TEST_CASE("the offer that completes the row publishes it")
{
  CaptureCorrelator c{2};

  REQUIRE(c.offer(0, 3, 1000) == -1);
  REQUIRE(c.setsPublished() == 0); // one member is still missing

  REQUIRE(c.offer(1, 7, 1100) == -1);
  REQUIRE(c.setsPublished() == 1);

  const auto a = c.group().take(0, 100);
  const auto b = c.group().take(1, 100);
  REQUIRE(a.slot == 3);
  REQUIRE(b.slot == 7);
  REQUIRE(a.generation == b.generation);
}

TEST_CASE("arrival order does not decide which member is which")
{
  CaptureCorrelator c{2};
  c.offer(1, 7, 1100);
  c.offer(0, 3, 1000);

  REQUIRE(c.group().take(0, 100).slot == 3);
  REQUIRE(c.group().take(1, 100).slot == 7);
}

TEST_CASE("a member that outruns the rig gets its stale slot back")
{
  CaptureCorrelator c{2};

  REQUIRE(c.offer(0, 3, 1000) == -1);
  // Member 0 produced again before member 1 produced at all: slot 3 can no
  // longer belong to any set, so it must come back for requeueing rather than
  // be held until the driver runs out of buffers.
  REQUIRE(c.offer(0, 4, 1033) == 3);
  REQUIRE(c.displacedFrames() == 1);
  REQUIRE(c.setsPublished() == 0);

  REQUIRE(c.offer(1, 7, 1133) == -1);
  REQUIRE(c.setsPublished() == 1);

  // The set carries the frame that was still current, not the displaced one.
  REQUIRE(c.group().take(0, 100).slot == 4);
  REQUIRE(c.group().take(1, 100).slot == 7);
}

TEST_CASE("timestamps reach the group so real skew can be reported")
{
  CaptureCorrelator c{2};
  c.offer(0, 0, 1'000'000);
  c.offer(1, 1, 8'600'000); // the rig's constant 7.6 ms offset

  const auto a = c.group().take(0, 100);
  REQUIRE(a.slot == 0);
  // The group derives skew from the stamps the correlator forwarded.
  REQUIRE(c.group().maxSkewNs() == 7'600'000);
}

TEST_CASE("a stalled member holds the rig instead of publishing half a row")
{
  CaptureCorrelator c{2};

  // Member 1 never delivers. Member 0 keeps producing and keeps displacing
  // itself, handing every superseded slot back so the driver never starves.
  REQUIRE(c.offer(0, 1, 1000) == -1);
  REQUIRE(c.offer(0, 2, 1033) == 1);
  REQUIRE(c.offer(0, 3, 1066) == 2);
  REQUIRE(c.offer(0, 4, 1099) == 3);

  // Nothing is published: a partial row would advance the group's generation
  // while its newest complete set stood still, and once that gap reaches the
  // ring depth the last good set is condemned as lapped -- turning one stalled
  // sensor into a dark rig.
  REQUIRE(c.setsPublished() == 0);
  REQUIRE(c.displacedFrames() == 3);
  REQUIRE(c.group().take(0, 100).slot == -1);
}

TEST_CASE("a recovered member resumes completing sets")
{
  CaptureCorrelator c{2};
  c.offer(0, 1, 1000);
  c.offer(0, 2, 1033);
  c.offer(0, 3, 1066);
  REQUIRE(c.setsPublished() == 0);

  // Member 1 comes back: the row completes from member 0's *current* frame,
  // not from any of the ones it displaced while waiting.
  c.offer(1, 9, 1106);
  REQUIRE(c.setsPublished() == 1);
  REQUIRE(c.group().take(0, 200).slot == 3);
  REQUIRE(c.group().take(1, 200).slot == 9);
}

TEST_CASE("draining returns exactly what each member was holding")
{
  CaptureCorrelator c{3};
  c.offer(0, 5, 1000);
  c.offer(2, 6, 1000);

  int held[CaptureFrameSet::kMaxMembers];
  c.drain(held);
  REQUIRE(held[0] == 5);
  REQUIRE(held[1] == -1);
  REQUIRE(held[2] == 6);

  // Draining clears, so the next round starts from nothing rather than pairing
  // a fresh frame with one from before the teardown.
  c.offer(0, 8, 2000);
  c.offer(1, 9, 2000);
  REQUIRE(c.setsPublished() == 0);
  c.offer(2, 10, 2000);
  REQUIRE(c.setsPublished() == 1);
  REQUIRE(c.group().take(2, 100).slot == 10);
}

TEST_CASE("out-of-range members and empty slots are refused, not stored")
{
  CaptureCorrelator c{2};
  REQUIRE(c.offer(2, 4, 1000) == -1);  // member past the end
  REQUIRE(c.offer(99, 4, 1000) == -1);
  REQUIRE(c.offer(0, -1, 1000) == -1); // "no frame" is not an offer
  REQUIRE(c.setsPublished() == 0);

  c.offer(0, 1, 1000);
  c.offer(1, 2, 1000);
  REQUIRE(c.setsPublished() == 1);
}

TEST_CASE("concurrent producers never assemble a torn set")
{
  constexpr int kFrames = 2000;
  CaptureCorrelator c{2};

  // Each member's slots come from a disjoint range, so a set holding two slots
  // from the same range is proof that one thread's offer landed in the other
  // member's cell.
  auto run = [&](std::size_t member, int base) {
    for(int i = 0; i < kFrames; ++i)
      c.offer(member, base + (i % 16), std::uint64_t(i) * 33'000'000);
  };

  std::thread t0{run, 0, 0};
  std::thread t1{run, 1, 100};
  t0.join();
  t1.join();

  REQUIRE(c.setsPublished() > 0);
  // Every published set must be well-formed: member 0's slot from 0..15 and
  // member 1's from 100..115, never the reverse.
  const auto a = c.group().take(0, 1);
  const auto b = c.group().take(1, 1);
  if(a.slot >= 0)
    REQUIRE(a.slot < 16);
  if(b.slot >= 0)
    REQUIRE(b.slot >= 100);
  REQUIRE(c.setsPublished() + c.displacedFrames() <= 2 * kFrames);
}
