#include <Media/Libav.hpp>

#if SCORE_HAS_LIBAV
#include <Video/FrameQueue.hpp>
#include <Video/VideoInterface.hpp>

#if defined(SCORE_LIBAV_FRAME_DEBUGGING)
#include <score/tools/Debug.hpp>

#include <ossia/detail/algorithms.hpp>
#endif

extern "C" {
#include <libavcodec/avcodec.h>
}
namespace Video
{

#if defined(SCORE_LIBAV_FRAME_DEBUGGING)
void frame_counters::allocate(AVFrame* av)
{
  std::lock_guard lock{mtx};
  auto it = ossia::find_if(allocated, [av](const auto& x) { return x.frame == av; });
  SCORE_ASSERT(it == allocated.end());
  allocated.push_back({av, boost::stacktrace::stacktrace{}});
}

void frame_counters::deallocate(AVFrame* av)
{
  std::lock_guard lock{mtx};
  auto it = ossia::find_if(allocated, [av](const auto& x) { return x.frame == av; });
  if(it == allocated.end())
  {
    qDebug() << "deallocating unknown frame?";
    return;
  }
  allocated.erase(it);

  it = ossia::find_if(allocated, [av](const auto& x) { return x.frame == av; });
  SCORE_ASSERT(it == allocated.end());
}

#endif

uint8_t* initFrameBuffer(AVFrame& frame, std::size_t bytes)
{
  // Here we need to copy the buffer.
  uint8_t* storage{};
  // Reuse allocated memory if any
  if(frame.data[0])
  {
    storage = frame.data[0];
  }
  else
  {
    // We got a new frame, init it
    auto buf = av_buffer_alloc(bytes);
    storage = buf->data;
    frame.buf[0] = buf;
    frame.data[0] = storage;
  }
  return storage;
}

FrameQueue::FrameQueue() { }

FrameQueue::~FrameQueue() { }

void FreeAVFrame::operator()(AVFrame* f) const noexcept
{
  SCORE_LIBAV_FRAME_DEALLOC_CHECK(f);
  av_frame_free(&f);
}
AVFramePointer FrameQueue::newFrame() noexcept
{
  // We were working on a frame in this thread (e.g. during a seek, when retrying with EAGAIN..)
  if(!m_decodeThreadFrameBuffer.empty())
  {
    auto f = m_decodeThreadFrameBuffer.back();
    m_decodeThreadFrameBuffer.pop_back();
    return AVFramePointer{f};
  }

  // Frames freed from the rendering thread
  {
    AVFrame* f{};
    if(released.try_dequeue(f))
    {
      return AVFramePointer{f};
    }
  }

  // We actually need to allocate :throw_up_emoji:
  auto new_frame = av_frame_alloc();
  new_frame->buf[0] = nullptr;
  new_frame->data[0] = nullptr;

  SCORE_LIBAV_FRAME_ALLOC_CHECK(new_frame);
  return AVFramePointer{new_frame};
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
  while(available.try_dequeue(f))
  {
    release(prev_f);
    prev_f = f;
  }

  return f;
}

AVFrame* FrameQueue::dequeue_one() noexcept
{
  AVFrame* f{};
  available.try_dequeue(f);
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

  if(auto to_discard = m_discardUntil.exchange(nullptr))
  {
    while(available.try_dequeue(f) && f != to_discard)
    {
      release(f);
    }

    return to_discard;
  }
  // We only want the latest frame
  while(available.try_dequeue(f))
  {
    if(prev_f)
      release(prev_f);
    prev_f = f;
  }
  return f;
}

AVFrame* FrameQueue::discard_and_dequeue_one() noexcept
{
  AVFrame* f{};

  if(auto to_discard = m_discardUntil.exchange(nullptr))
  {
    while(available.try_dequeue(f) && f != to_discard)
    {
      release(f);
    }

    return to_discard;
  }

  available.try_dequeue(f);
  return f;
}

void FrameQueue::release(AVFrame* frame) noexcept
{
  if(frame)
  {
    released.enqueue(frame);
  }
}

void FrameQueue::drain()
{
  {
    AVFrame* frame{};
    while(available.try_dequeue(frame))
    {
      SCORE_LIBAV_FRAME_DEALLOC_CHECK(frame);
      av_frame_free(&frame);
    }
  }

  // TODO we must check that this is safe as the queue
  // does not support dequeueing from the same thread as the
  // enqueuing
  {
    AVFrame* frame{};
    while(released.try_dequeue(frame))
    {
      SCORE_LIBAV_FRAME_DEALLOC_CHECK(frame);
      av_frame_free(&frame);
    }
  }

  for(auto frame : m_decodeThreadFrameBuffer)
  {
    SCORE_LIBAV_FRAME_DEALLOC_CHECK(frame);
    av_frame_free(&frame);
  }
  m_decodeThreadFrameBuffer.clear();
}

}
#endif
