#include "FileWatch.hpp"

#include <score/application/ApplicationServices.hpp>

#include <QFile>
#include <QDateTime>
#include <QApplication>
#include <QFileInfo>

namespace score
{
static int64_t get_mtime(const QString& path)
{
  QFileInfo f{path};
  if(!f.exists())
    return 0;

  return f.lastModified().toMSecsSinceEpoch();
}

FileWatch::FileWatch() noexcept
{
  m_thread = score::ThreadPool::instance().acquireThread();

  // Timer runs in main thread
  m_timer = startTimer(500);

  this->moveToThread(m_thread);
}

FileWatch::~FileWatch()
{
  score::ThreadPool::instance().releaseThread();
}

FileWatch& FileWatch::instance()
{
  static std::once_flag init{};
  std::call_once(init, [] {
    score::AppServices().filewatch.emplace();
  });
  return *score::AppServices().filewatch;
}

void FileWatch::add(QString path, comparable_function cb)
{
  auto mtime = get_mtime(path);
  std::lock_guard l{m_mtx};

  if(auto it = m_map.find(path); it != m_map.end())
  {
    it->second.functions.push_back(std::move(cb));
  }
  else
  {
    m_map.emplace(path, watch{.mtime = mtime, .functions { std::move(cb) }});
  }
}

void FileWatch::remove(QString path, comparable_function cb)
{
  std::lock_guard l{m_mtx};
  auto& v = m_map[path].functions;
  auto it = std::find(v.begin(), v.end(), cb);
  if(it != v.end())
  {
    v.erase(it);
    if(v.empty())
      m_map.erase(path);
  }
}

void FileWatch::timerEvent(QTimerEvent* ev)
{
  if(!m_thread)
    return;

  // This executes in the thread:
  QMetaObject::invokeMethod(this, [this] {
    map_type cur_map;
    {
      std::lock_guard l{m_mtx};
      cur_map = m_map;
    }

    struct pair { QString path; int64_t mtime; };
    boost::container::small_vector<pair, 4> vec;

    for(auto& [path, watch] : cur_map)
    {
      QFileInfo f{path};
      if(!f.exists())
        continue;

      if(auto mtime = get_mtime(path); mtime > watch.mtime)
      {
        vec.push_back(pair{path, mtime});
        watch.mtime = mtime;
        for(auto& f : watch.functions)
          (*f)();
      }
    }

    {
      std::lock_guard l{m_mtx};
      for(auto& elt : vec)
      {
        if(auto it = m_map.find(elt.path); it != m_map.end())
          it->second.mtime = elt.mtime;
      }
    }
  }, Qt::QueuedConnection);
}

}
