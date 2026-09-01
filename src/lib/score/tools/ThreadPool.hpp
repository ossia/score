#pragma once
#include <QAbstractEventDispatcher>
#include <QCoreApplication>
#include <QThread>

#include <blockingconcurrentqueue.h>
#include <score_lib_base_export.h>
#include <smallfun.hpp>

#include <memory>
#include <mutex>
#include <thread>
namespace score
{
class SCORE_LIB_BASE_EXPORT ThreadPool
{
public:
  ThreadPool();
  ~ThreadPool();

  static ThreadPool& instance();

  QThread* acquireThread();
  void releaseThread();

  //! How many threads have actually been started, as opposed to reserved.
  //! Handing out one used to start all of them.
  int startedThreadCount() const noexcept;

  //! Stop and join the workers. Called when the application goes away: doing it
  //! on the last release meant joining them from the UI thread mid-session.
  void shutdown();

private:
  // Acquired from the UI thread, released from whoever held the last reference
  // to the work -- so every field below has two writers.
  mutable std::mutex m_mutex;
  std::unique_ptr<QThread[]> m_threads;
  std::unique_ptr<bool[]> m_started;
  int m_numThreads{};
  int m_currentThread{};

  int m_inFlight = 0;
};

class SCORE_LIB_BASE_EXPORT TaskPool
{
public:
  TaskPool();
  ~TaskPool();
  static TaskPool& instance();

  template <typename F>
  void post(F&& func)
  {
    m_queue.enqueue(std::forward<F>(func));
  }

private:
  using task = smallfun::function<
      void(),
#if defined(_MSC_VER) && !defined(NDEBUG)
      256,
#else
      128,
#endif
      std::max((int)8, (int)std::max(alignof(std::function<void()>), alignof(double))),
      smallfun::Methods::Move>;
  moodycamel::BlockingConcurrentQueue<task> m_queue;
  std::array<std::thread, 4> m_threads;
  std::atomic_bool m_running{};
};
}
