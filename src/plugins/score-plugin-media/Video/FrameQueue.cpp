#include <Media/Libav.hpp>
#if SCORE_HAS_LIBAV

#include <Video/FrameQueue.hpp>

extern "C"
{
#include <libavcodec/avcodec.h>
}

namespace Video
{

AVFrame* FrameQueue::newFrame() noexcept
{
  AVFrame* f{};
  if(released.try_dequeue(f))
    return f;
  return av_frame_alloc();
}

void FrameQueue::enqueue(AVFrame* f)
{
  available.enqueue(f);
}

AVFrame* FrameQueue::dequeue() noexcept
{
  AVFrame* f{};
  AVFrame* prev_f{};

  // We only want the latest frame
  while(available.try_dequeue(f)) {
    if(prev_f)
      release(prev_f);
    prev_f = f;
  }
  return f;
}

void FrameQueue::release(AVFrame* frame) noexcept
{
  released.enqueue(frame);
}

void FrameQueue::drain()
{
  AVFrame* frame{};
  while (available.try_dequeue(frame))
  {
    av_frame_free(&frame);
  }

  // TODO we must check that this is safe as the queue
  // does not support dequeueing from the same thread as the
  // enqueuing
  while (released.try_dequeue(frame))
  {
    av_frame_free(&frame);
  }
}

}
#endif
