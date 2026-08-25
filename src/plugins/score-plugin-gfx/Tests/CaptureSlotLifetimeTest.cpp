// Unit tests for the two lock-free handoffs every capture strategy is built
// on: CaptureStrategyCommon.hpp's CaptureSlotPublisher / BorrowedSlotTracker,
// and VideoCaptureStrategy.hpp's VideoCaptureSlotRing format seqlock.
//
// Both are pure logic -- atomics and a small array -- and both are wrong in
// ways that do not crash. A borrowed slot handed back one acquisition too early
// is a frame the producer's device overwrites while the GPU is still sampling
// it; one handed back too late, or never, starves the producer's queue instead.
// A format generation bumped when nothing changed makes the render thread
// rebuild every GPU resource it owns once per frame.
//
// No QRhi and no application.

#include <Gfx/Graph/interop/CaptureStrategyCommon.hpp>
#include <Gfx/Graph/interop/VideoCaptureStrategy.hpp>

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <thread>
#include <vector>

using score::gfx::interop::BorrowedSlotTracker;
using score::gfx::interop::CaptureSlotPublisher;
using score::gfx::interop::VideoCaptureSlotRing;

namespace
{
constexpr std::uint32_t bit(int i) noexcept
{
  return 1u << unsigned(i);
}
} // namespace

TEST_CASE("the publisher hands over the newest slot, once")
{
  CaptureSlotPublisher p;
  CHECK(p.consume() == -1);

  p.publish(3);
  CHECK(p.consume() == 3);
  CHECK(p.consume() == -1);

  // The renderer polls; a producer that publishes twice between two polls has
  // made the older frame stale, and stale is what must be dropped.
  p.publish(1);
  p.publish(2);
  CHECK(p.consume() == 2);

  p.publish(4);
  p.reset();
  CHECK(p.consume() == -1);
}

TEST_CASE("a borrowed slot is not returned before the GPU is done with it")
{
  BorrowedSlotTracker t;
  t.retireDepth = 3;

  CHECK(t.ingest(0) == -1);
  CHECK(t.acquire() == 0);
  // Slot 0 is what the renderer is displaying: nothing may come back yet.
  CHECK(t.takeReturned() == 0u);

  // Each acquisition displaces the slot bound before it. Slot 0 is displaced by
  // the acquisition of slot 1 and becomes returnable only `retireDepth`
  // acquisitions later; returning it at either of the two in between corrupts
  // the frame on screen.
  CHECK(t.ingest(1) == -1);
  CHECK(t.acquire() == 1);
  CHECK(t.takeReturned() == 0u);

  CHECK(t.ingest(2) == -1);
  CHECK(t.acquire() == 2);
  CHECK(t.takeReturned() == 0u);

  CHECK(t.ingest(3) == -1);
  CHECK(t.acquire() == 3);
  CHECK(t.takeReturned() == 0u);

  CHECK(t.ingest(4) == -1);
  CHECK(t.acquire() == 4);
  CHECK(t.takeReturned() == bit(0));

  // ...and then one per acquisition, in order.
  CHECK(t.ingest(5) == -1);
  CHECK(t.acquire() == 5);
  CHECK(t.takeReturned() == bit(1));
}

TEST_CASE("a shallower retire depth returns sooner, still never immediately")
{
  BorrowedSlotTracker t;
  t.retireDepth = 1;

  CHECK(t.ingest(0) == -1);
  CHECK(t.acquire() == 0);
  CHECK(t.takeReturned() == 0u);

  // Displaced here, so not returnable here either: the acquisition that
  // displaces a slot is the one whose frame may still be sampling it.
  CHECK(t.ingest(1) == -1);
  CHECK(t.acquire() == 1);
  CHECK(t.takeReturned() == 0u);

  CHECK(t.ingest(2) == -1);
  CHECK(t.acquire() == 2);
  CHECK(t.takeReturned() == bit(0));
}

TEST_CASE("a slot that was never bound goes straight back")
{
  BorrowedSlotTracker t;
  t.retireDepth = 3;

  CHECK(t.ingest(4) == -1);
  // The renderer never polled, so slot 4 was never displayed: it is the
  // producer's again immediately, or a stalled renderer starves the queue.
  CHECK(t.ingest(5) == 4);
  CHECK(t.takeReturned() == bit(4));

  // Re-publishing the slot already pending is not a displacement: handing it
  // back would give the producer a buffer the renderer is about to bind.
  CHECK(t.ingest(5) == -1);
  CHECK(t.takeReturned() == 0u);
}

TEST_CASE("reset gives a re-inited strategy a clean tracker")
{
  BorrowedSlotTracker t;
  t.retireDepth = 2;

  t.ingest(0);
  t.acquire();
  t.ingest(1);
  t.acquire();
  t.ingest(2);

  t.reset();

  CHECK(t.takeReturned() == 0u);
  CHECK(t.acquire() == -1);
  // held was cleared too, so the first acquisition after a reset retires
  // nothing.
  CHECK(t.ingest(7) == -1);
  CHECK(t.acquire() == 7);
  CHECK(t.takeReturned() == 0u);
}

// A retire depth deeper than the retirement array is the case the bound in
// acquire() exists for: the pending retirements pile up and the write must
// stop at kMaxSlots rather than run off the end.
TEST_CASE("more pending retirements than the array holds")
{
  BorrowedSlotTracker t;
  t.retireDepth = 1000;

  for(int i = 0; i < 500; ++i)
  {
    const auto slot = std::size_t(i % BorrowedSlotTracker::kMaxSlots);
    CHECK(t.ingest(slot) == -1);
    CHECK(t.acquire() == int(slot));
    CHECK(t.takeReturned() == 0u);
  }
}

TEST_CASE("a fresh capture ring has no format yet")
{
  VideoCaptureSlotRing r;
  const auto f = r.loadFormat();
  CHECK(f.generation == 0u);
  CHECK(f.pixfmt == -1);
  CHECK(f.width == 0);
  CHECK(f.height == 0);
  CHECK(r.latestFrameId.load() == 0u);
}

TEST_CASE("publishing an unchanged geometry does not bump the generation")
{
  VideoCaptureSlotRing r;

  REQUIRE(r.publishFormat(1920, 1080, 23 /*AV_PIX_FMT_UYVY422*/, 59.94));
  const auto first = r.loadFormat();
  CHECK(first.generation == 2u);
  CHECK(first.width == 1920);
  CHECK(first.height == 1080);
  CHECK(first.pixfmt == 23);
  CHECK(first.rate == 59.94);

  // The capture thread calls this per frame; a generation bump the render
  // thread has to answer means reallocating every size-dependent GPU resource
  // it owns, every frame.
  CHECK_FALSE(r.publishFormat(1920, 1080, 23, 59.94));
  CHECK(r.loadFormat().generation == 2u);

  // The rate alone is not part of the geometry test, so a rate-only change is
  // dropped rather than triggering a rebuild.
  CHECK_FALSE(r.publishFormat(1920, 1080, 23, 50.0));
  CHECK(r.loadFormat().rate == 59.94);

  REQUIRE(r.publishFormat(1280, 720, 23, 50.0));
  const auto second = r.loadFormat();
  CHECK(second.generation == 4u);
  CHECK(second.generation % 2 == 0);
  CHECK(second.width == 1280);
  CHECK(second.height == 720);
  CHECK(second.rate == 50.0);
}

// What the seqlock is there for: the reader must never assemble a tuple out of
// two different publications -- 1920 wide and 720 high is a geometry no capture
// device ever had, and the render thread would allocate for it.
TEST_CASE("a reader never sees half of two geometries")
{
  VideoCaptureSlotRing r;
  std::atomic<bool> stop{false};
  std::atomic<int> torn{0};
  std::atomic<int> reads{0};

  std::thread reader{[&] {
    while(!stop.load(std::memory_order_relaxed))
    {
      const auto f = r.loadFormat();
      const bool a = f.width == 1920 && f.height == 1080 && f.pixfmt == 23
                     && f.rate == 59.94;
      const bool b = f.width == 1280 && f.height == 720 && f.pixfmt == 2
                     && f.rate == 50.0;
      const bool unset = f.generation == 0;
      if(!a && !b && !unset)
        torn.fetch_add(1, std::memory_order_relaxed);
      reads.fetch_add(1, std::memory_order_relaxed);
    }
  }};

  // Wait for the reader to get through at least one iteration before racing it.
  // std::thread's constructor does not promise the body has begun, so on a
  // machine where thread start-up is slower than 100000 publishes the writer
  // finished and set stop before the reader ever looped -- reads == 0 and the
  // test failed without anything being wrong. That is what it did on Windows,
  // on every backend, in 0.03s.
  while(reads.load(std::memory_order_relaxed) == 0)
    std::this_thread::yield();

  for(int i = 0; i < 100000; ++i)
  {
    if(i % 2)
      r.publishFormat(1920, 1080, 23, 59.94);
    else
      r.publishFormat(1280, 720, 2, 50.0);
  }

  stop.store(true, std::memory_order_relaxed);
  reader.join();

  CHECK(reads.load() > 0);
  CHECK(torn.load() == 0);
  CHECK(r.loadFormat().generation % 2 == 0);
}
