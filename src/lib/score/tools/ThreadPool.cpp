#include <score/application/ApplicationServices.hpp>
#include <score/tools/ThreadPool.hpp>

#include <QDebug>

#include <thread>
#if __has_include(<sys/resource.h>)
#include <sys/resource.h>
#endif
#include <ossia/detail/thread.hpp>

namespace score
{
ThreadPool::ThreadPool() { }

ThreadPool::~ThreadPool()
{
  shutdown();
}

ThreadPool& ThreadPool::instance()
{
  static std::once_flag init{};
  std::call_once(init, [] { score::AppServices().threadpool.emplace(); });
  return *score::AppServices().threadpool;
}

QThread* ThreadPool::acquireThread()
{
  std::lock_guard lock{m_mutex};
  if(!m_threads)
  {
    m_numThreads = std::thread::hardware_concurrency();
    if(m_numThreads > 2)
      m_numThreads = m_numThreads / 2;
    if(m_numThreads < 2)
      m_numThreads = 2;

#if defined(__EMSCRIPTEN__)
    // A browser hands out a fixed pool of workers, and creating one past it has
    // to return to the event loop -- which a thread being started from the main
    // thread cannot do. It deadlocks instead.
    if(m_numThreads > 2)
      m_numThreads = 2;
#endif

    m_threads = std::make_unique<QThread[]>(m_numThreads);
    m_started = std::make_unique<bool[]>(m_numThreads);
    m_currentThread = 0;

    // The threads now outlive every release, so the application going away is
    // what ends them. A worker whose event loop is still running once
    // QCoreApplication is gone warns and has nothing left to run on.
    if(auto* app = QCoreApplication::instance())
    {
      QObject::connect(app, &QCoreApplication::aboutToQuit, app, [this] { shutdown(); });
      QObject::connect(app, &QObject::destroyed, app, [this] { shutdown(); });
    }
  }

  // Started as they are handed out: starting all of them to answer one request
  // costs the whole machine's worth of threads, and a 5 MB stack each, for a
  // single waveform.
  QThread& t = m_threads[m_currentThread];
  if(!m_started[m_currentThread])
  {
#if __has_include(<sys/resource.h>) && !defined(__EMSCRIPTEN__)
    ::rlimit lim{0, 0};
    getrlimit(RLIMIT_STACK, &lim);

    if(lim.rlim_cur > t.stackSize())
      t.setStackSize(lim.rlim_cur);
#endif
    t.setObjectName(QString("ossia uitask %1").arg(m_currentThread));
    t.start();
    t.setPriority(QThread::Priority::HighPriority);
    m_started[m_currentThread] = true;
  }

  m_currentThread++;
  m_currentThread = m_currentThread % m_numThreads;
  m_inFlight++;
  return &t;
}

int ThreadPool::startedThreadCount() const noexcept
{
  std::lock_guard lock{m_mutex};
  if(!m_started)
    return 0;
  int n = 0;
  for(int i = 0; i < m_numThreads; i++)
    if(m_started[i])
      ++n;
  return n;
}

void ThreadPool::shutdown()
{
  std::lock_guard lock{m_mutex};
  if(!m_threads)
    return;

  for(int i = 0; i < m_numThreads; i++)
    if(m_started[i])
      m_threads[i].quit();
  for(int i = 0; i < m_numThreads; i++)
    if(m_started[i])
      m_threads[i].wait();

  m_threads.reset();
  m_started.reset();
  m_currentThread = 0;
}

void ThreadPool::releaseThread()
{
  std::lock_guard lock{m_mutex};
  if(m_inFlight > 0)
    m_inFlight--;

  // The threads outlive the last release, and go away with the pool. Stopping
  // them here meant joining them from whoever released last -- usually the UI
  // thread, which then waited on work it was itself supposed to let run. In a
  // browser the wait never ends: a worker only finishes starting once the main
  // thread returns to the event loop.
}

TaskPool::TaskPool()
{
  m_running = true;
  int max_n = std::thread::hardware_concurrency();
  if(max_n > 8)
    max_n = 4;
  else if(max_n >= 4)
    max_n = 2;
  else
    max_n = 1;

  int i = 0;
  for(auto& t : m_threads)
  {
    t = std::thread{[this, i = i++] {
      ossia::set_thread_name("ossia task " + std::to_string(i));
      task t{};
      while(m_running)
      {
        if(m_queue.wait_dequeue_timed(t, 100000))
        {
          try
          {
            t();
          }
          catch(const std::exception& e)
          {
            qDebug() << "TaskPool: task threw:" << e.what();
          }
          catch(...)
          {
            qDebug() << "TaskPool: task threw an unknown exception";
          }
        }
      }
    }};
    if(i >= max_n)
      break;
  }
}

TaskPool::~TaskPool()
{
  m_running = false;
  for(auto& t : m_threads)
  {
    if(t.joinable())
      t.join();
  }
}

TaskPool& TaskPool::instance()
{
  static std::once_flag init{};
  std::call_once(init, [] { score::AppServices().taskpool.emplace(); });
  return *score::AppServices().taskpool;
}

}
