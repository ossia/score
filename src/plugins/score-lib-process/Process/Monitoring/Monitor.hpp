#pragma once
#include <ossia/detail/triple_buffer.hpp>

#include <score_lib_process_export.h>

#include <atomic>
#include <cstdint>
#include <utility>

namespace Process
{
/**
 * State shared between a running process and the views that display it.
 *
 * The process writes it from its execution thread, the views read it on the
 * GUI thread; whatever a subclass adds must be safe for exactly that: no
 * locks, no allocation on the writing side. Both sides hold it through a
 * shared_ptr, so it outlives whichever of the two goes away first.
 *
 * Several views may show the same process at once (a looping process is
 * drawn once per loop). What a view takes out of the monitor, events or the
 * latest frame, must then reach every one of them: use EventRing and
 * LatestValue, which give all the views refreshed in the same tick of the
 * monitor clock (see monitorFrame()) the same content.
 */
struct SCORE_LIB_PROCESS_EXPORT Monitor
{
  Monitor() noexcept;
  Monitor(const Monitor&) = delete;
  Monitor& operator=(const Monitor&) = delete;
  virtual ~Monitor();

  //! How many views are showing the process right now. With none, the process
  //! skips what only serves display; measurements that feed its outlets go on.
  std::atomic<int> viewers{0};

  bool watched() const noexcept
  {
    return viewers.load(std::memory_order_relaxed) > 0;
  }
};

/**
 * GUI thread: the tick of the clock that refreshes the monitor views, while
 * it refreshes them; 0 outside of that. Readers that consume what they read
 * keep it for the other views of the same tick.
 */
SCORE_LIB_PROCESS_EXPORT uint64_t monitorFrame() noexcept;

/**
 * The latest frame a process published, for any number of views.
 *
 * ossia::triple_buffer gives a frame to the first reader only; this keeps it
 * on the GUI side for the tick in which it was taken, so that every view
 * refreshed in that tick gets it. The writing side is the triple buffer's.
 */
template <typename T>
class LatestValue
{
public:
  LatestValue() = default;
  explicit LatestValue(const T& init)
      : m_buffer{init}
      , m_last{init}
  {
  }
  LatestValue(const LatestValue&) = delete;
  LatestValue& operator=(const LatestValue&) = delete;

  //! Writer
  template <typename U>
  void produce(U&& v)
  {
    m_buffer.produce(std::forward<U>(v));
  }

  //! Reader: true and the newest frame in `out` when one came since the
  //! previous tick; in one tick, every caller gets the same answer.
  bool consume(T& out)
  {
    const uint64_t frame = monitorFrame();
    if(frame == 0)
      return m_buffer.consume(out);
    if(frame != m_frame)
    {
      m_frame = frame;
      m_fresh = m_buffer.consume(m_last);
    }
    if(m_fresh)
      out = m_last;
    return m_fresh;
  }

private:
  ossia::triple_buffer<T> m_buffer;
  T m_last{};
  uint64_t m_frame{};
  bool m_fresh{};
};
}
