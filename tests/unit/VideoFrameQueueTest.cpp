// Video::FrameQueue -- the ownership boundary between the libav decode thread
// and the render thread.
//
// Three behaviours carry real consequences and none of them needs a codec:
//
//  - the free-list. newFrame() must recycle a frame the render thread released
//    rather than allocating; a leak here is one AVFrame per decoded frame.
//  - "latest frame wins". dequeue() drains everything and returns the newest,
//    releasing the ones it skipped back into the pool. dequeue_one() is the
//    single-step form the video process uses when it must not drop frames.
//  - the seek discard marker. set_discard_frame(f) says "everything before f is
//    stale". discard_and_dequeue*() must return f only if f is actually still
//    in the queue: returning a frame the queue does not hold is a double
//    ownership of the pixels a zero-copy GPU upload may be reading (the
//    ordering comment in VideoDecoder::seek_impl() exists for exactly this).
//
// Frames are plain av_frame_alloc()ed carriers here: the queue never looks at
// pixel data, only at pointer identity.

#include <Video/FrameQueue.hpp>

#include <catch2/catch_test_macros.hpp>

#include <set>
#include <vector>

extern "C" {
#include <libavutil/frame.h>
}

using namespace Video;

namespace
{
// A frame the queue can hand around. Ownership goes to the queue.
AVFrame* freshFrame(int marker)
{
  auto* f = av_frame_alloc();
  f->buf[0] = nullptr;
  f->data[0] = nullptr;
  f->pkt_dts = marker;
  return f;
}
}

TEST_CASE("newFrame recycles released frames before allocating", "[video][framequeue]")
{
  FrameQueue q;

  auto a = q.newFrame();
  auto b = q.newFrame();
  REQUIRE(a);
  REQUIRE(b);
  CHECK(a.get() != b.get());

  AVFrame* aRaw = a.release();
  AVFrame* bRaw = b.release();

  // The render thread is done with them.
  q.release(aRaw);
  q.release(bRaw);

  // ...so the decode thread must get the same two carriers back, in some order,
  // instead of allocating new ones.
  auto c = q.newFrame();
  auto d = q.newFrame();
  std::set<AVFrame*> recycled{c.get(), d.get()};
  CHECK(recycled == std::set<AVFrame*>{aRaw, bRaw});

  // The pool is empty again: the next one is a genuine allocation.
  auto e = q.newFrame();
  CHECK(recycled.count(e.get()) == 0);

  // Hand everything to the queue so drain() owns it.
  q.enqueue(c.release());
  q.enqueue(d.release());
  q.enqueue(e.release());
  q.drain();
}

TEST_CASE("release(nullptr) is a no-op, not a queued null", "[video][framequeue]")
{
  FrameQueue q;
  q.release(nullptr);

  // If the null had been queued, newFrame() would try to av_frame_unref() it.
  auto f = q.newFrame();
  REQUIRE(f);
  q.enqueue(f.release());
  q.drain();
}

TEST_CASE("enqueue_decoding_error frames come back first", "[video][framequeue]")
{
  // The decode thread parks a frame it could not fill (EAGAIN mid-seek) and
  // must get that exact carrier back on the retry, ahead of the render
  // thread's released pool -- otherwise a retry loop allocates every round.
  FrameQueue q;

  auto released = q.newFrame();
  AVFrame* releasedRaw = released.release();
  q.release(releasedRaw);

  // Allocated outside the pool so that the two sources stay distinguishable.
  AVFrame* parkedRaw = freshFrame(7);
  q.enqueue_decoding_error(parkedRaw);

  auto again = q.newFrame();
  CHECK(again.get() == parkedRaw);

  auto then = q.newFrame();
  CHECK(then.get() == releasedRaw);

  q.enqueue(again.release());
  q.enqueue(then.release());
  q.drain();
}

TEST_CASE("dequeue returns the newest frame and recycles the rest",
          "[video][framequeue]")
{
  FrameQueue q;

  std::vector<AVFrame*> pushed;
  for(int i = 0; i < 5; i++)
  {
    pushed.push_back(freshFrame(i));
    q.enqueue(pushed.back());
  }
  CHECK(q.size() == 5);

  AVFrame* got = q.dequeue();
  REQUIRE(got == pushed.back());
  CHECK(q.size() == 0);

  // The four skipped frames went back into the pool, not into the void: four
  // newFrame() calls must produce them and allocate nothing.
  q.release(got);
  std::set<AVFrame*> recovered;
  std::vector<AVFramePointer> keep;
  for(int i = 0; i < 5; i++)
  {
    keep.push_back(q.newFrame());
    recovered.insert(keep.back().get());
  }
  CHECK(recovered == std::set<AVFrame*>(pushed.begin(), pushed.end()));

  for(auto& f : keep)
    q.enqueue(f.release());
  q.drain();
}

TEST_CASE("dequeue on an empty queue yields nothing", "[video][framequeue]")
{
  FrameQueue q;
  CHECK(q.dequeue() == nullptr);
  CHECK(q.dequeue_one() == nullptr);
  CHECK(q.discard_and_dequeue() == nullptr);
  CHECK(q.discard_and_dequeue_one() == nullptr);
  CHECK(q.size() == 0);
}

TEST_CASE("dequeue_one advances one frame at a time", "[video][framequeue]")
{
  FrameQueue q;
  AVFrame* a = freshFrame(0);
  AVFrame* b = freshFrame(1);
  AVFrame* c = freshFrame(2);
  q.enqueue(a);
  q.enqueue(b);
  q.enqueue(c);

  CHECK(q.dequeue_one() == a);
  CHECK(q.dequeue_one() == b);
  CHECK(q.dequeue_one() == c);
  CHECK(q.dequeue_one() == nullptr);

  q.release(a);
  q.release(b);
  q.release(c);
  q.drain();
}

TEST_CASE("the discard marker drops everything before it", "[video][framequeue]")
{
  // Post-seek shape: three stale frames are still queued, then the seek's first
  // good frame is enqueued and published as the marker.
  FrameQueue q;
  AVFrame* stale[3] = {freshFrame(0), freshFrame(1), freshFrame(2)};
  for(auto* f : stale)
    q.enqueue(f);

  AVFrame* target = freshFrame(99);
  q.enqueue(target);
  q.set_discard_frame(target);

  CHECK(q.discard_and_dequeue_one() == target);
  CHECK(q.size() == 0);

  // The stale ones were recycled rather than leaked.
  q.release(target);
  std::set<AVFrame*> pool;
  std::vector<AVFramePointer> keep;
  for(int i = 0; i < 4; i++)
  {
    keep.push_back(q.newFrame());
    pool.insert(keep.back().get());
  }
  CHECK(pool
        == std::set<AVFrame*>{stale[0], stale[1], stale[2], target});

  for(auto& f : keep)
    q.enqueue(f.release());
  q.drain();
}

TEST_CASE("the marker is consumed once", "[video][framequeue]")
{
  FrameQueue q;
  AVFrame* target = freshFrame(1);
  q.enqueue(target);
  q.set_discard_frame(target);

  CHECK(q.discard_and_dequeue_one() == target);

  // A second call must behave as a plain dequeue: the marker is gone.
  AVFrame* next = freshFrame(2);
  q.enqueue(next);
  CHECK(q.discard_and_dequeue_one() == next);

  q.release(target);
  q.release(next);
  q.drain();
}

// The guard the ordering comment in VideoDecoder::seek_impl() is about: if the
// marker is published for a frame that is NOT in `available` (already consumed
// by a normal dequeue, or a torn set_discard_frame/enqueue), returning it would
// hand the render thread a frame the decode thread still owns.
TEST_CASE("a marker for an absent frame yields nothing, not the frame",
          "[video][framequeue]")
{
  FrameQueue q;
  AVFrame* consumed = freshFrame(1);
  q.enqueue(consumed);
  REQUIRE(q.dequeue_one() == consumed);

  AVFrame* stale = freshFrame(2);
  q.enqueue(stale);

  // Marker names a frame the queue no longer holds.
  q.set_discard_frame(consumed);

  CHECK(q.discard_and_dequeue_one() == nullptr);
  CHECK(q.discard_and_dequeue() == nullptr);
  // The queue was drained looking for it, and the frames it passed were
  // recycled rather than returned.
  CHECK(q.size() == 0);

  q.release(consumed);
  q.drain();
  (void)stale;
}

TEST_CASE("discard_and_dequeue without a marker is latest-frame-wins",
          "[video][framequeue]")
{
  FrameQueue q;
  AVFrame* a = freshFrame(0);
  AVFrame* b = freshFrame(1);
  AVFrame* c = freshFrame(2);
  q.enqueue(a);
  q.enqueue(b);
  q.enqueue(c);

  CHECK(q.discard_and_dequeue() == c);
  CHECK(q.size() == 0);

  q.release(c);
  q.drain();
}

TEST_CASE("drain frees the queued, the released and the parked",
          "[video][framequeue]")
{
  // All three internal holders must be emptied: whatever drain() misses is a
  // leak on every file close. Only observable as "the queue is empty
  // afterwards" plus a clean run under the sanitizers.
  FrameQueue q;
  q.enqueue(freshFrame(0));
  q.enqueue(freshFrame(1));
  q.release(freshFrame(2));
  q.enqueue_decoding_error(freshFrame(3));

  q.drain();

  CHECK(q.size() == 0);
  CHECK(q.dequeue() == nullptr);

  // The pool is empty too: a newFrame() now allocates instead of returning one
  // of the four freed carriers.
  auto f = q.newFrame();
  REQUIRE(f);
  q.enqueue(f.release());
  q.drain();
}

TEST_CASE("drain tolerates the same frame held twice", "[video][framequeue]")
{
  // The decode-error buffer and the released queue can both name a frame in a
  // teardown race; drain() de-duplicates through a flat_set, and a double
  // av_frame_free() here would be a hard crash rather than a failed assertion.
  FrameQueue q;
  AVFrame* f = freshFrame(0);
  q.release(f);
  q.enqueue_decoding_error(f);

  q.drain();
  CHECK(q.size() == 0);
}

TEST_CASE("initFrameBuffer allocates once and then reuses", "[video][framequeue]")
{
  AVFrame* f = av_frame_alloc();
  f->buf[0] = nullptr;
  f->data[0] = nullptr;

  uint8_t* first = initFrameBuffer(*f, 1024);
  REQUIRE(first != nullptr);
  CHECK(f->data[0] == first);
  REQUIRE(f->buf[0] != nullptr);
  CHECK(f->buf[0]->size >= 1024u);

  // A second call on a frame that already carries storage must not allocate --
  // that is the whole point of the pool above.
  AVBufferRef* buf = f->buf[0];
  uint8_t* second = initFrameBuffer(*f, 1024);
  CHECK(second == first);
  CHECK(f->buf[0] == buf);

  // The storage is writable for the size that was asked for.
  first[0] = 0x5A;
  first[1023] = 0xA5;
  CHECK(first[0] == 0x5A);
  CHECK(first[1023] == 0xA5);

  av_frame_free(&f);
}
