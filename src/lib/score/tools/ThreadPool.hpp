#pragma once
#include <QAbstractEventDispatcher>
#include <QThread>

#include <score_lib_base_export.h>

#include <memory>
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
  static TaskPool& instance();

  void init();

  template <typename F>
  void submit(F&& func)
  {
    init();
    auto t = threads.back();

    auto disp = QAbstractEventDispatcher::instance(t);
    QMetaObject::invokeMethod(disp, std::forward<F>(func));
  }

private:
  std::vector<QThread*> threads;
};
}
