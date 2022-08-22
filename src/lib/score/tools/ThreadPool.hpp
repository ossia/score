#pragma once
#include <QAbstractEventDispatcher>
#include <QThread>

#include <blockingconcurrentqueue.h>
#include <score_lib_base_export.h>
#include <smallfun.hpp>

#include <memory>
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

private:
  std::unique_ptr<QThread[]> m_threads;
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
  std::array<std::thread, 2> m_threads;
  std::atomic_bool m_running{};
};
}
