// Unit tests for TripleBufferIndex, the lock-free slot rotation the
// TextureShare backends (OpenGL / Vulkan / D3D11 / D3D12 / Metal) share.
//
// The class is the whole safety argument of TextureShare: the producer renders
// into `write` while the consumer samples `read`, and nothing but this state
// machine keeps those two indices apart. A rotation that hands the same slot to
// both does not crash and does not fail a readback deterministically -- it
// tears one frame in N, on a GPU, in another thread.
//
// No QRhi, no application: the index is seven bits in one atomic.

#include <Gfx/Graph/interop/TripleBufferIndex.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <atomic>
#include <cstdint>
#include <thread>
#include <vector>

using score::gfx::TripleBufferIndex;

namespace
{

// The documented state machine, kept independently of the implementation so a
// change to the packing has to agree with something written down.
struct Model
{
  int write{0};
  int ready{1};
  int read{2};
  bool hasNew{false};

  void publish() noexcept
  {
    std::swap(write, ready);
    hasNew = true;
  }

  // Returns the index the consumer should be handed.
  int acquire() noexcept
  {
    if(!hasNew)
      return read;
    std::swap(ready, read);
    hasNew = false;
    return read;
  }

  bool distinct() const noexcept
  {
    return write != ready && ready != read && write != read;
  }
};

constexpr int kNeverWritten = -1;

}

TEST_CASE("a fresh index hands out three distinct slots", "[gfx][interop][triplebuffer]")
{
  TripleBufferIndex idx;

  CHECK(idx.acquireWriteIndex() == 0);
  CHECK_FALSE(idx.hasNewFrameAvailable());
  // No frame has been published, so this must not consume anything.
  CHECK(idx.acquireReadIndex() != idx.acquireWriteIndex());
  CHECK(idx.acquireWriteIndex() == 0);
}

TEST_CASE("publish makes exactly one frame available", "[gfx][interop][triplebuffer]")
{
  TripleBufferIndex idx;

  const int first = idx.acquireWriteIndex();
  idx.publishWriteIndex();
  REQUIRE(idx.hasNewFrameAvailable());

  CHECK(idx.acquireReadIndex() == first);
  CHECK_FALSE(idx.hasNewFrameAvailable());

  // A second acquire with nothing new keeps the consumer on the same slot
  // rather than rotating it onto a stale one.
  CHECK(idx.acquireReadIndex() == first);
  CHECK_FALSE(idx.hasNewFrameAvailable());
}

TEST_CASE("the producer never re-enters the slot the consumer holds",
          "[gfx][interop][triplebuffer]")
{
  TripleBufferIndex idx;

  int held = idx.acquireReadIndex();
  for(int i = 0; i < 64; ++i)
  {
    const int w = idx.acquireWriteIndex();
    REQUIRE(w != held);
    idx.publishWriteIndex();
    // The consumer keeps its texture until its next acquire; one publish while
    // it holds a slot must not target that slot.
    REQUIRE(idx.acquireWriteIndex() != held);
    held = idx.acquireReadIndex();
  }
}

TEST_CASE("the consumer is served the newest published frame, not a queued one",
          "[gfx][interop][triplebuffer]")
{
  TripleBufferIndex idx;
  std::array<int, 3> slot{kNeverWritten, kNeverWritten, kNeverWritten};

  int frame = 0;
  // Producer runs three frames ahead without the consumer ever looking.
  for(int i = 0; i < 3; ++i)
  {
    slot[idx.acquireWriteIndex()] = ++frame;
    idx.publishWriteIndex();
  }

  const int r = idx.acquireReadIndex();
  CHECK(slot[r] == frame);
}

TEST_CASE("every reachable state keeps the three roles distinct",
          "[gfx][interop][triplebuffer]")
{
  // Exhaustive over every publish/acquire interleaving of depth 14 (16384
  // sequences), each step checked against the documented model.
  constexpr int kDepth = 14;
  for(std::uint32_t seq = 0; seq < (1u << kDepth); ++seq)
  {
    TripleBufferIndex idx;
    Model model;

    for(int step = 0; step < kDepth; ++step)
    {
      REQUIRE(model.distinct());
      REQUIRE(idx.acquireWriteIndex() == model.write);
      REQUIRE(idx.hasNewFrameAvailable() == model.hasNew);

      if(seq & (1u << step))
      {
        idx.publishWriteIndex();
        model.publish();
      }
      else
      {
        const int got = idx.acquireReadIndex();
        REQUIRE(got == model.acquire());
        REQUIRE(got >= 0);
        REQUIRE(got < 3);
      }
    }
    REQUIRE(model.distinct());
  }
}

TEST_CASE("a producer and a consumer thread never share a slot",
          "[gfx][interop][triplebuffer]")
{
  TripleBufferIndex idx;
  std::array<std::atomic<bool>, 3> heldByConsumer{};
  for(auto& h : heldByConsumer)
    h.store(false, std::memory_order_relaxed);

  std::atomic<bool> stop{false};
  std::atomic<int> collisions{0};
  std::atomic<int> badIndex{0};

  constexpr int kFrames = 200000;

  std::thread producer([&] {
    for(int i = 0; i < kFrames; ++i)
    {
      const int w = idx.acquireWriteIndex();
      if(w < 0 || w >= 3)
      {
        badIndex.fetch_add(1, std::memory_order_relaxed);
        continue;
      }
      // The whole render happens here in the real backend; the consumer must
      // not be sampling this slot for its whole duration.
      if(heldByConsumer[w].load(std::memory_order_acquire))
        collisions.fetch_add(1, std::memory_order_relaxed);
      idx.publishWriteIndex();
    }
    stop.store(true, std::memory_order_release);
  });

  std::thread consumer([&] {
    int cur = -1;
    while(!stop.load(std::memory_order_acquire))
    {
      // Release the previous texture, then ask for the next one: that is the
      // TextureShare contract ("valid until the next acquireConsumerTexture").
      if(cur >= 0)
        heldByConsumer[cur].store(false, std::memory_order_release);
      const int r = idx.acquireReadIndex();
      if(r < 0 || r >= 3)
      {
        badIndex.fetch_add(1, std::memory_order_relaxed);
        cur = -1;
        continue;
      }
      heldByConsumer[r].store(true, std::memory_order_release);
      cur = r;
    }
    if(cur >= 0)
      heldByConsumer[cur].store(false, std::memory_order_release);
  });

  producer.join();
  consumer.join();

  CHECK(badIndex.load() == 0);
  CHECK(collisions.load() == 0);
}

// ---------------------------------------------------------------------------
// FINDING (expected RED): acquireReadIndex() never returns -1.
//
// Its own comment says "Returns the read index, or -1 if no new frame", and
// every backend is written to that contract:
//
//   int readSlot = m_tripleBuffer.acquireReadIndex();
//   if(readSlot < 0 || readSlot >= 3)
//     return m_consumerTextureWrappers[m_currentReadSlot];
//
// The branch is dead. Before the producer has published anything the call
// returns 2 -- a real index into slots that were created with QRhi::create()
// and never rendered into -- so TextureShare::acquireConsumerTexture(), which
// documents "the most recently completed texture, or nullptr if none ready",
// hands the consumer undefined texture contents instead of nullptr. The only
// thing that distinguishes "not ready" from "ready" today is the separate
// hasNewFrame() query, which the acquire path does not consult.
//
// Marked !shouldfail: the assertion below states the documented contract, so
// it goes green the day the code meets it.
// ---------------------------------------------------------------------------
TEST_CASE("no frame yet means no slot", "[gfx][interop][triplebuffer][!shouldfail]")
{
  TripleBufferIndex idx;
  REQUIRE_FALSE(idx.hasNewFrameAvailable());
  CHECK(idx.acquireReadIndex() == -1);
}

TEST_CASE("a slot is never handed out before it was written",
          "[gfx][interop][triplebuffer][!shouldfail]")
{
  TripleBufferIndex idx;
  std::array<int, 3> slot{kNeverWritten, kNeverWritten, kNeverWritten};

  // Consumer polls before the producer has rendered anything, which is what
  // every startup looks like when the two threads race.
  const int r = idx.acquireReadIndex();
  CHECK((r < 0 || slot[r] != kNeverWritten));
}
