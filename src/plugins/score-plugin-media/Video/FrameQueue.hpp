#pragma once
#include <Media/Libav.hpp>

#if SCORE_HAS_LIBAV
#include <Video/VideoInterface.hpp>

#include <ossia/detail/lockfree_queue.hpp>

#include <score_plugin_media_export.h>

#if defined(SCORE_LIBAV_FRAME_DEBUGGING)
#include <mutex>
#define BOOST_STACKTRACE_USE_BACKTRACE 1
#include <boost/stacktrace.hpp>
#endif

#include <atomic>
#include <vector>

extern "C" {
#include <libavutil/frame.h>
}
namespace Video
{

#if defined(SCORE_LIBAV_FRAME_DEBUGGING)
inline struct frame_counters
{
  std::mutex mtx;
  struct fc
  {
    AVFrame* frame{};
    boost::stacktrace::stacktrace st;
  };
  std::vector<fc> allocated;
  void allocate(AVFrame*);
  void deallocate(AVFrame*);
} frame_counts;
#define SCORE_LIBAV_FRAME_ALLOC_CHECK(f) frame_counts.allocate(f)
#define SCORE_LIBAV_FRAME_DEALLOC_CHECK(f) frame_counts.deallocate(f)
#else
#define SCORE_LIBAV_FRAME_ALLOC_CHECK(f) \
  do                                     \
  {                                      \
  } while(0)
#define SCORE_LIBAV_FRAME_DEALLOC_CHECK(f) \
  do                                       \
  {                                        \
  } while(0)
#endif

struct SCORE_PLUGIN_MEDIA_EXPORT FrameQueue
{
public:
  FrameQueue();
  ~FrameQueue();

  FrameQueue(const FrameQueue&) = delete;
  FrameQueue(FrameQueue&&) = delete;
  FrameQueue& operator=(const FrameQueue&) = delete;
  FrameQueue& operator=(FrameQueue&&) = delete;

  AVFramePointer newFrame() noexcept;

  void enqueue_decoding_error(AVFrame* f);
  void enqueue(AVFrame* f);
  AVFrame* dequeue() noexcept;
  AVFrame* dequeue_one() noexcept;
  AVFrame* discard_and_dequeue() noexcept;
  AVFrame* discard_and_dequeue_one() noexcept;

  //! The next discard_and_dequeue* drops the queued frames that come before
  //! a seek's landing frame: those of an earlier generation, and those of the
  //! current one shown before `pts`. The first frame past them is returned and
  //! ends the discard; until one is queued the discard waits. A date rather
  //! than the landing frame itself, so that a consumer which took that frame
  //! before the marker was set keeps the frames after it. Decoding thread
  //! only.
  void set_discard_before(int64_t pts) noexcept;
  void release(AVFrame* frame) noexcept;

  //! Frames enqueued from now on belong to a new seek. Each frame carries the
  //! generation it was enqueued in (frameGeneration).
  int start_generation() noexcept { return ++m_generation; }
  void drain();

  std::size_t size() const noexcept { return available.size_approx(); }

  //! Follows one frame given out by the decoding thread: watched() is null
  //! once newFrame() has handed that frame out again, the consumer having
  //! taken and released it. Decoding thread only.
  void watch(AVFrame* f) noexcept { m_watched = f; }
  AVFrame* watched() const noexcept { return m_watched; }

private:
  AVFramePointer newFrameImpl() noexcept;

  ossia::mpmc_queue<AVFrame*> available;
  ossia::mpmc_queue<AVFrame*> released;

  //! The discard marker: true when `f` comes before it. Consumer side.
  struct Discard
  {
    int64_t pts{};
    int generation{};
    uint32_t sequence{};
  };
  bool pendingDiscard(Discard& d) const noexcept;
  static bool before(const AVFrame& f, const Discard& d) noexcept;
  template <typename Dequeue>
  AVFrame* dequeueDiscarding(const Dequeue& dequeue) noexcept;

  std::vector<AVFrame*> m_decodeThreadFrameBuffer;
  AVFrame* m_watched{};
  // A sequence lock around the marker: odd while the decoding thread writes
  // it; a marker is pending while its sequence is not the one the consumer
  // last finished.
  std::atomic<uint32_t> m_discardSequence{};
  std::atomic<int64_t> m_discardPts{};
  std::atomic_int m_discardGeneration{};
  std::atomic<uint32_t> m_discardDone{};
  std::atomic_int m_generation{};
};

//! The seek generation a frame was enqueued in (FrameQueue::start_generation).
inline int frameGeneration(const AVFrame& f) noexcept
{
  return int(reinterpret_cast<intptr_t>(f.opaque));
}

SCORE_PLUGIN_MEDIA_EXPORT
uint8_t* initFrameBuffer(AVFrame& frame, std::size_t bytes);

}
#endif
