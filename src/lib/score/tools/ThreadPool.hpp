#pragma once
#include <score_lib_base_export.h>
#include <QThread>
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
}
