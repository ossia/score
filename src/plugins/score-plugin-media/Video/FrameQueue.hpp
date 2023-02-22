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
struct AVFrame;
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

  void set_discard_frame(AVFrame*);
  void release(AVFrame* frame) noexcept;
  void drain();

  std::size_t size() const noexcept { return available.size_approx(); }

private:
  ossia::mpmc_queue<AVFrame*> available;
  ossia::mpmc_queue<AVFrame*> released;

  std::vector<AVFrame*> m_decodeThreadFrameBuffer;
  std::atomic<AVFrame*> m_discardUntil{};
};

SCORE_PLUGIN_MEDIA_EXPORT
uint8_t* initFrameBuffer(AVFrame& frame, std::size_t bytes);

}
#endif
