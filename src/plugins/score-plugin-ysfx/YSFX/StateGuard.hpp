#pragma once
#include <atomic>
#include <thread>

namespace YSFX
{
/**
 * Keeps a script's code from running on two threads at once. Reading or
 * writing its state runs its @serialize section, which shares the script's
 * memory with the @sample code the audio thread runs.
 *
 * The audio thread never waits: when the state is in use it skips its tick.
 * The other side waits for the tick being computed to end.
 */
class StateGuard
{
public:
  //! On the audio thread
  [[nodiscard]] bool try_enter() noexcept
  {
    return !m_busy.exchange(true, std::memory_order_acquire);
  }

  //! Anywhere else
  void enter() noexcept
  {
    while(m_busy.exchange(true, std::memory_order_acquire))
      std::this_thread::yield();
  }

  void leave() noexcept { m_busy.store(false, std::memory_order_release); }

private:
  std::atomic_bool m_busy{false};
};

class StateLock
{
public:
  explicit StateLock(StateGuard& g) noexcept
      : m_guard{g}
  {
    m_guard.enter();
  }
  ~StateLock() { m_guard.leave(); }
  StateLock(const StateLock&) = delete;
  StateLock& operator=(const StateLock&) = delete;

private:
  StateGuard& m_guard;
};
}
