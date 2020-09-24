#include <score/tools/ThreadPool.hpp>
#include <thread>

namespace score
{
ThreadPool::ThreadPool()
{
}

ThreadPool::~ThreadPool()
{
  if(m_threads)
  {
    for(int i = 0; i < m_numThreads; i++)
    {
      m_threads[i].quit();
    }
    for(int i = 0; i < m_numThreads; i++)
    {
      m_threads[i].wait();
    }

    m_threads.reset();
  }
}

ThreadPool& ThreadPool::instance() {
  static ThreadPool threads;
  return threads;
}

QThread* ThreadPool::acquireThread()
{
  if(!m_threads)
  {
    m_numThreads = std::thread::hardware_concurrency();
    if(m_numThreads > 2)
      m_numThreads = m_numThreads / 2;
    if(m_numThreads < 2)
      m_numThreads = 2;
    m_threads = std::make_unique<QThread[]>(m_numThreads);

    for(int i = 0; i < m_numThreads; i++)
    {
      m_threads[i].start();
    }
    m_currentThread = 0;
  }

  QThread& t = m_threads[m_currentThread];
  m_currentThread++;
  m_currentThread = m_currentThread % m_numThreads;
  m_inFlight ++;
  return &t;
}

void ThreadPool::releaseThread()
{
  m_inFlight--;

  if(m_inFlight == 0)
  {
    for(int i = 0; i < m_numThreads; i++)
    {
      m_threads[i].quit();
    }
    for(int i = 0; i < m_numThreads; i++)
    {
      m_threads[i].wait();
    }

    m_threads.reset();
  }
}
}
