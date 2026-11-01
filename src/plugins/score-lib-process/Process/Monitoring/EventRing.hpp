#pragma once
#include <Process/Monitoring/Monitor.hpp>

#include <ossia/detail/lockfree_queue.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace Process
{
/**
 * Discrete events from the execution thread to the GUI, none lost while
 * they fit: a bounded single-producer queue, which does not allocate when
 * pushing. A full queue drops what is pushed and counts it, so that the view
 * can say so.
 *
 * Every view refreshed in the same tick of the monitor clock gets the same
 * events (see monitorFrame()); outside of a tick, drain() takes them.
 *
 * T is copied in and out; keep it small and trivially copyable.
 */
template <typename T, std::size_t N>
class EventRing
{
public:
  EventRing()
      : m_queue{N}
  {
    m_staged.reserve(N);
  }
  EventRing(const EventRing&) = delete;
  EventRing& operator=(const EventRing&) = delete;

  //! Writer: false when the ring is full and `e` was dropped.
  bool push(const T& e) noexcept
  {
    if(m_queue.try_enqueue(e))
      return true;
    m_dropped.fetch_add(1, std::memory_order_relaxed);
    return false;
  }

  //! Reader: calls f with every event pushed since the last drain, or, in a
  //! tick of the monitor clock, since the previous tick.
  template <typename F>
  int drain(F&& f)
  {
    const uint64_t frame = monitorFrame();
    T e;
    if(frame == 0)
    {
      int n = 0;
      while(m_queue.try_dequeue(e))
      {
        f(std::as_const(e));
        ++n;
      }
      return n;
    }

    if(frame != m_frame)
    {
      m_frame = frame;
      m_staged.clear();
      while(m_queue.try_dequeue(e))
        m_staged.push_back(e);
    }
    for(const T& s : m_staged)
      f(s);
    return int(m_staged.size());
  }

  uint64_t dropped() const noexcept { return m_dropped.load(std::memory_order_relaxed); }

private:
  ossia::spsc_queue<T> m_queue;
  std::atomic<uint64_t> m_dropped{0};

  // GUI side
  std::vector<T> m_staged;
  uint64_t m_frame{};
};
}
