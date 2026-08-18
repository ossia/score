// Unit tests for Gfx/Graph/interop/PacedFramePump.{hpp,cpp}: the ring and
// consumer thread every card-output addon paces its submits through.
//
// The invariant worth guarding is not throughput, it is bookkeeping. A frame
// pointer handed to the pump carries a resource the vendor has to reclaim -- a
// pooled host frame, a card frame acquired through a FrameMemoryProvider -- so
// every pointer that goes in must come back out exactly once, either through
// submit() or through discard(), and never through both. A pointer that comes
// in and never comes back is a leaked pool slot, and the pool runs dry some
// minutes later with nothing pointing at the cause.
//
// The vendor tick is driven by the test rather than by a clock: waitForTick()
// blocks on a semaphore the test releases, so the drain-to-newest and retry
// paths are reached deterministically instead of by sleeping and hoping.

#include <Gfx/Graph/interop/PacedFramePump.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <mutex>
#include <semaphore>
#include <thread>
#include <vector>

using score::gfx::interop::PacedFramePump;
using namespace std::chrono_literals;

namespace
{
// Stand-ins for frame pointers; the pump never dereferences them.
void* frame(int i) noexcept
{
  static char storage[16]{};
  return storage + i;
}

std::vector<void*> sorted(std::vector<void*> v)
{
  std::sort(v.begin(), v.end());
  return v;
}

struct Recorder
{
  std::mutex mutex;
  std::vector<void*> submitted;
  std::vector<void*> discarded;
  std::counting_semaphore<> submits{0};

  bool submit(void* p)
  {
    {
      std::lock_guard lock{mutex};
      submitted.push_back(p);
    }
    submits.release();
    return true;
  }
  void discard(void* p)
  {
    std::lock_guard lock{mutex};
    discarded.push_back(p);
  }
  bool awaitSubmit() { return submits.try_acquire_for(5s); }
};

// The vendor tick, under the test's control. A hook that never returns would
// make stop() hang, so this one times out the way the header asks vendors to.
struct TickGate
{
  std::counting_semaphore<> permits{0};
  std::atomic<int> calls{0};

  bool wait()
  {
    calls.fetch_add(1, std::memory_order_relaxed);
    return permits.try_acquire_for(100ms);
  }
  void tick() { permits.release(); }
};
} // namespace

TEST_CASE("one tick submits the newest frame and discards the rest")
{
  Recorder rec;
  TickGate gate;
  PacedFramePump pump{
      {.waitForTick = [&] { return gate.wait(); },
       .canAccept = nullptr,
       .submit = [&](void* p) { return rec.submit(p); },
       .discard = [&](void* p) { rec.discard(p); }},
      3};
  pump.start();

  REQUIRE(pump.push(frame(1)));
  REQUIRE(pump.push(frame(2)));
  REQUIRE(pump.push(frame(3)));

  gate.tick();
  REQUIRE(rec.awaitSubmit());
  pump.stop();

  CHECK(rec.submitted == std::vector<void*>{frame(3)});
  CHECK(rec.discarded == std::vector<void*>{frame(1), frame(2)});
  CHECK(pump.goodXfers() == 1u);
}

// The pump parks on frame arrival, not on the vendor tick, so a hook that
// spends a scarce resource per call cannot lose one to an idle pump.
TEST_CASE("an idle pump never asks for a tick")
{
  Recorder rec;
  TickGate gate;
  PacedFramePump pump{
      {.waitForTick = [&] { return gate.wait(); },
       .canAccept = nullptr,
       .submit = [&](void* p) { return rec.submit(p); },
       .discard = [&](void* p) { rec.discard(p); }},
      3};
  pump.start();

  // Longer than the pump's own 100 ms frames-available timeout, so the loop has
  // gone round several times with nothing pending.
  std::this_thread::sleep_for(350ms);
  CHECK(gate.calls.load() == 0);

  REQUIRE(pump.push(frame(1)));
  gate.tick();
  REQUIRE(rec.awaitSubmit());
  pump.stop();

  CHECK(gate.calls.load() >= 1);
  CHECK(rec.submitted == std::vector<void*>{frame(1)});
  CHECK(rec.discarded.empty());
}

TEST_CASE("a tick that times out leaves the frame queued")
{
  Recorder rec;
  std::atomic<int> ticks{0};
  std::counting_semaphore<> permits{0};
  PacedFramePump pump{
      {.waitForTick =
           [&] {
             // The first tick times out; the pending frame must survive it.
             if(ticks.fetch_add(1, std::memory_order_relaxed) == 0)
               return false;
             return permits.try_acquire_for(5s);
           },
       .canAccept = nullptr,
       .submit = [&](void* p) { return rec.submit(p); },
       .discard = [&](void* p) { rec.discard(p); }},
      3};
  pump.start();

  REQUIRE(pump.push(frame(1)));
  permits.release();
  REQUIRE(rec.awaitSubmit());
  pump.stop();

  CHECK(ticks.load() >= 2);
  CHECK(rec.submitted == std::vector<void*>{frame(1)});
  CHECK(rec.discarded.empty());
  CHECK(pump.goodXfers() == 1u);
}

// Card-side back-pressure is a reason to wait, not a reason to throw the frame
// away: the vendor owns the memory behind the pointer either way, and dropping
// it here would be a frame the card never showed and the pool never got back.
TEST_CASE("a card that cannot take the frame yet is waited out")
{
  Recorder rec;
  std::atomic<int> ticks{0};
  std::atomic<int> accepts{0};
  PacedFramePump pump{
      {.waitForTick =
           [&] {
             ticks.fetch_add(1, std::memory_order_relaxed);
             return true;
           },
       // Free before the drain, busy at the submit gate right after it, free
       // again on the retry.
       .canAccept =
           [&] { return accepts.fetch_add(1, std::memory_order_relaxed) != 1; },
       .submit = [&](void* p) { return rec.submit(p); },
       .discard = [&](void* p) { rec.discard(p); }},
      3};
  pump.start();

  REQUIRE(pump.push(frame(1)));
  REQUIRE(rec.awaitSubmit());
  pump.stop();

  CHECK(rec.submitted == std::vector<void*>{frame(1)});
  CHECK(rec.discarded.empty());
  CHECK(pump.goodXfers() == 1u);
  CHECK(pump.drops() == 1u);
  CHECK(ticks.load() == 1);
}

TEST_CASE("a full ring hands the rejected frame back")
{
  Recorder rec;
  PacedFramePump pump{
      {.waitForTick = [] { return false; },
       .canAccept = nullptr,
       .submit = [&](void* p) { return rec.submit(p); },
       .discard = [&](void* p) { rec.discard(p); }},
      2};

  CHECK(pump.push(frame(1)));
  CHECK(pump.push(frame(2)));
  CHECK_FALSE(pump.push(frame(3)));
  CHECK_FALSE(pump.push(nullptr));

  CHECK(rec.submitted.empty());
  CHECK(rec.discarded == std::vector<void*>{frame(3)});
  CHECK(pump.drops() == 1u);
}

// The whole point of the discard hook. Frames rejected at push, frames a newer
// one superseded, and frames still queued when the pump stops are three
// different code paths, and a pointer leaked on any of them is a pool slot that
// never comes back.
TEST_CASE("every frame comes back exactly once, by one route or the other")
{
  SECTION("nothing ever ticks")
  {
    Recorder rec;
    PacedFramePump pump{
        {.waitForTick =
             [] {
               std::this_thread::sleep_for(1ms);
               return false;
             },
         .canAccept = nullptr,
         .submit = [&](void* p) { return rec.submit(p); },
         .discard = [&](void* p) { rec.discard(p); }},
        2};
    pump.start();

    CHECK(pump.push(frame(1)));
    CHECK(pump.push(frame(2)));
    CHECK_FALSE(pump.push(frame(3)));
    pump.stop();

    CHECK(rec.submitted.empty());
    CHECK(
        sorted(rec.discarded)
        == sorted({frame(1), frame(2), frame(3)}));
  }

  SECTION("one tick, then stop")
  {
    Recorder rec;
    TickGate gate;
    PacedFramePump pump{
        {.waitForTick = [&] { return gate.wait(); },
         .canAccept = nullptr,
         .submit = [&](void* p) { return rec.submit(p); },
         .discard = [&](void* p) { rec.discard(p); }},
        3};
    pump.start();

    CHECK(pump.push(frame(1)));
    CHECK(pump.push(frame(2)));
    CHECK(pump.push(frame(3)));
    gate.tick();
    REQUIRE(rec.awaitSubmit());
    CHECK(pump.push(frame(4)));
    pump.stop();

    auto all = rec.discarded;
    all.insert(all.end(), rec.submitted.begin(), rec.submitted.end());
    CHECK(
        sorted(all) == sorted({frame(1), frame(2), frame(3), frame(4)}));

    for(void* p : rec.submitted)
    {
      INFO("a submitted frame was discarded as well");
      CHECK(
          std::find(rec.discarded.begin(), rec.discarded.end(), p)
          == rec.discarded.end());
    }
  }
}

TEST_CASE("start and stop are idempotent")
{
  Recorder rec;
  PacedFramePump pump{
      {.waitForTick =
           [] {
             std::this_thread::sleep_for(1ms);
             return false;
           },
       .canAccept = nullptr,
       .submit = [&](void* p) { return rec.submit(p); },
       .discard = [&](void* p) { rec.discard(p); }},
      3};
  pump.start();
  pump.start();
  pump.stop();
  pump.stop();
  CHECK(pump.goodXfers() == 0u);
}
