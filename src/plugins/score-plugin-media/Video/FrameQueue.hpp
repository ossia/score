#pragma once
#include <Media/Libav.hpp>
#if SCORE_HAS_LIBAV

#include <ossia/detail/lockfree_queue.hpp>
#include <score_plugin_media_export.h>

extern "C" {
struct AVFrame;
}
namespace Video
{
struct SCORE_PLUGIN_MEDIA_EXPORT FrameQueue
{
public:
  AVFrame* newFrame() noexcept;

  void enqueue(AVFrame* f);
  AVFrame* dequeue() noexcept;

  void release(AVFrame* frame) noexcept;
  void drain();

  std::size_t size() const noexcept { return available.size_approx(); }
private:
  ossia::spsc_queue<AVFrame*, 16> available;
  ossia::spsc_queue<AVFrame*, 16> released;
};
}
#endif
