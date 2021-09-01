#pragma once
#include <Media/Libav.hpp>
#if SCORE_HAS_LIBAV
#include <Video/VideoInterface.hpp>
#include <ossia/detail/lockfree_queue.hpp>
#include <vector>
#include <atomic>

#include <score_plugin_media_export.h>

extern "C"
{
  struct AVFrame;
}
namespace Video
{
struct SCORE_PLUGIN_MEDIA_EXPORT FrameQueue
{
public:
  AVFramePointer newFrame() noexcept;

  void enqueue_decoding_error(AVFrame* f);
  void enqueue(AVFrame* f);
  AVFrame* dequeue() noexcept;
  AVFrame* discard_and_dequeue() noexcept;

  void set_discard_frame(AVFrame*);
  void release(AVFrame* frame) noexcept;
  void drain();

  std::size_t size() const noexcept { return available.size_approx(); }

private:
  ossia::spsc_queue<AVFrame*, 16> available;
  ossia::spsc_queue<AVFrame*, 16> released;

  std::vector<AVFrame*> m_decodeThreadFrameBuffer;
  std::atomic<AVFrame*> m_discardUntil{};
};
}
#endif
