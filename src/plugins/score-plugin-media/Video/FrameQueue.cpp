#include <Media/Libav.hpp>
#if SCORE_HAS_LIBAV

#include <Video/VideoInterface.hpp>
#include <Video/FrameQueue.hpp>

extern "C"
{
#include <libavcodec/avcodec.h>
}

namespace Video
{

AVFramePointer FrameQueue::newFrame() noexcept
{
  // We were working on a frame in this thread (e.g. during a seek, when retrying with EAGAIN..)
  if (!m_decodeThreadFrameBuffer.empty())
  {
    auto f = m_decodeThreadFrameBuffer.back();
    m_decodeThreadFrameBuffer.pop_back();
    return AVFramePointer{f};
  }

  // Frames freed from the rendering thread
  {
  AVFrame* f{};
  if (released.try_dequeue(f))
    return AVFramePointer{f};
  }

  // We actually need to allocate :throw_up_emoji:
  return AVFramePointer{av_frame_alloc()};
}

void FrameQueue::enqueue_decoding_error(AVFrame* f)
{
  this->m_decodeThreadFrameBuffer.push_back(f);
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
  while (available.try_dequeue(f))
  {
    if (prev_f)
      release(prev_f);
    prev_f = f;
  }
  return f;
}

void FrameQueue::set_discard_frame(AVFrame* f)
{
  m_discardUntil.exchange(f);
}
AVFrame* FrameQueue::discard_and_dequeue() noexcept
{
  AVFrame* f{};
  AVFrame* prev_f{};

  if (auto to_discard = m_discardUntil.exchange(nullptr))
  {
    while (available.try_dequeue(f) && f != to_discard)
    {
      release(f);
    }

    return to_discard;
  }
  // We only want the latest frame
  while (available.try_dequeue(f))
  {
    if (prev_f)
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
  {
    AVFrame* frame{};
    while (available.try_dequeue(frame))
    {
      av_frame_free(&frame);
    }
  }

  // TODO we must check that this is safe as the queue
  // does not support dequeueing from the same thread as the
  // enqueuing
  {
    AVFrame* frame{};
    while (released.try_dequeue(frame))
    {
      av_frame_free(&frame);
    }
  }


  for(auto f : m_decodeThreadFrameBuffer)
  {
    av_frame_free(&f);
  }
  m_decodeThreadFrameBuffer.clear();
}

}
#endif
