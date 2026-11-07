#include <Media/Libav.hpp>

#if SCORE_HAS_LIBAV
#include <Video/FrameQueue.hpp>
#include <Video/VideoInterface.hpp>

#include <ossia/detail/flat_set.hpp>
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
  // Reuse the buffer a recycled frame still carries, but only when it is still
  // big enough: a mid-stream format change grows `bytes`, and the plane
  // pointers a caller wrote into data[] do not have to start at the buffer.
  if(frame.buf[0] && frame.buf[0]->size >= bytes)
  {
    frame.data[0] = frame.buf[0]->data;
    return frame.data[0];
  }

  if(frame.buf[0])
    av_buffer_unref(&frame.buf[0]);

  auto buf = av_buffer_alloc(bytes);
  if(!buf)
    return nullptr;

  frame.buf[0] = buf;
  frame.data[0] = buf->data;
  return buf->data;
}

FrameQueue::FrameQueue() { }

// Frames still in the queues are owned by the queue, even if the owner never
// called drain(). drain() is idempotent.
FrameQueue::~FrameQueue()
{
  drain();
}

void FreeAVFrame::operator()(AVFrame* f) const noexcept
{
  SCORE_LIBAV_FRAME_DEALLOC_CHECK(f);
  av_frame_free(&f);
}
AVFramePointer FrameQueue::newFrame() noexcept
{
  auto f = newFrameImpl();
  if(f.get() == m_watched)
    m_watched = nullptr;
  return f;
}

AVFramePointer FrameQueue::newFrameImpl() noexcept
{
  // We were working on a frame in this thread (e.g. during a seek, when retrying with EAGAIN..)
  if(!m_decodeThreadFrameBuffer.empty())
  {
    auto f = m_decodeThreadFrameBuffer.back();
    m_decodeThreadFrameBuffer.pop_back();
    av_frame_unref(f);
    return AVFramePointer{f};
  }

  // Frames freed from the rendering thread
  {
    AVFrame* f{};
    if(released.try_dequeue(f))
    {
      av_frame_unref(f);
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
  if(f)
    f->opaque = reinterpret_cast<void*>(intptr_t(m_generation.load()));
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

void FrameQueue::set_discard_before(int64_t pts) noexcept
{
  const uint32_t s = m_discardSequence.load(std::memory_order_relaxed);
  m_discardSequence.store(s + 1, std::memory_order_relaxed);
  std::atomic_thread_fence(std::memory_order_release);
  m_discardPts.store(pts, std::memory_order_relaxed);
  m_discardGeneration.store(m_generation.load(), std::memory_order_relaxed);
  m_discardSequence.store(s + 2, std::memory_order_release);
}

bool FrameQueue::pendingDiscard(Discard& d) const noexcept
{
  for(;;)
  {
    const uint32_t s = m_discardSequence.load(std::memory_order_acquire);
    if(s & 1)
      continue;
    if(s == m_discardDone.load(std::memory_order_relaxed))
      return false;
    d.pts = m_discardPts.load(std::memory_order_relaxed);
    d.generation = m_discardGeneration.load(std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_acquire);
    if(m_discardSequence.load(std::memory_order_relaxed) == s)
    {
      d.sequence = s;
      return true;
    }
  }
}

bool FrameQueue::before(const AVFrame& f, const Discard& d) noexcept
{
  const int g = frameGeneration(f);
  if(g != d.generation)
    return g < d.generation;
  return f.pts != AV_NOPTS_VALUE && d.pts != AV_NOPTS_VALUE && f.pts < d.pts;
}

template <typename Dequeue>
AVFrame* FrameQueue::dequeueDiscarding(const Dequeue& dequeue) noexcept
{
  Discard d;
  if(!pendingDiscard(d))
    return dequeue();

  AVFrame* f{};
  while(available.try_dequeue(f))
  {
    if(before(*f, d))
    {
      release(f);
      continue;
    }
    m_discardDone.store(d.sequence, std::memory_order_relaxed);
    return f;
  }
  return nullptr;
}

AVFrame* FrameQueue::discard_and_dequeue() noexcept
{
  return dequeueDiscarding([this] { return dequeue(); });
}

AVFrame* FrameQueue::discard_and_dequeue_one() noexcept
{
  return dequeueDiscarding([this] { return dequeue_one(); });
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
  thread_local ossia::flat_set<AVFrame*> frames_to_free;
  frames_to_free.clear();
  {
    AVFrame* frame{};
    while(available.try_dequeue(frame))
    {
      SCORE_LIBAV_FRAME_DEALLOC_CHECK(frame);
      frames_to_free.insert(frame);
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
      frames_to_free.insert(frame);
    }
  }

  for(auto frame : m_decodeThreadFrameBuffer)
  {
    SCORE_LIBAV_FRAME_DEALLOC_CHECK(frame);
    frames_to_free.insert(frame);
  }
  m_decodeThreadFrameBuffer.clear();
  m_watched = nullptr;

  for(auto frame : frames_to_free)
    av_frame_free(&frame);
  frames_to_free.clear();
}

}
#endif
