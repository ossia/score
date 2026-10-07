#include "FileWatch.hpp"

#include <score/application/ApplicationServices.hpp>

#include <ossia-qt/invoke.hpp>

#include <QApplication>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QThread>

namespace score
{
static int64_t get_mtime(const QString& path)
{
  QFileInfo f{path};
  if(!f.exists())
    return 0;

  return f.lastModified().toMSecsSinceEpoch();
}

// Carries the pool thread's affinity, and nothing else: the directory scan is
// posted to this object so it runs off the GUI thread, while the FileWatch --
// and therefore its timer -- stays on the thread that created it.
//
// It used to be the FileWatch itself that was moveToThread()'d onto the pool
// thread, and it was moved *after* startTimer(), so the 500 ms timer ended up
// registered in the pool thread's event dispatcher. ~Application then
// destroyed the FileWatch from the GUI thread (`svc.filewatch.reset()`), and
// Qt cannot unregister a timer from a foreign thread: ~QObjectPrivate only
// warns, "QObject::~QObject: Timers cannot be stopped from another thread",
// and leaves the registration in place. The dispatcher went on ticking a
// dangling QObject*, and the next tick dispatched a QTimerEvent into the freed
// block -- QTimerInfoList::activateTimers -> QCoreApplication::notifyInternal2
// on an object that no longer exists. That is an access violation on exit
// (0xC0000005 on Windows, SIGSEGV under ASan here), and it only fires when the
// pool thread is still pumping when the FileWatch is freed, which is why it
// was intermittent rather than systematic.
//
// Destroying this receiver also drops any scan still queued on it, so the
// lambda below cannot run against a half-destroyed FileWatch -- the same
// reason Gfx/Graph/RenderClock.hpp keeps a separate m_receiver.
class FileWatch::Worker : public QObject
{
};

FileWatch::FileWatch() noexcept
{
  m_thread = score::ThreadPool::instance().acquireThread();

  // The scan context lives on the pool thread; this object does not.
  m_worker = new Worker;
  m_worker->moveToThread(m_thread);

  // Timer runs in the thread that created us, and is killed there too: this
  // object never changes thread affinity.
  m_timer = startTimer(500);
}

FileWatch::~FileWatch()
{
  // Same thread as the constructor, so this really unregisters the timer
  // instead of warning and leaving it armed.
  if(m_timer != -1)
  {
    killTimer(m_timer);
    m_timer = -1;
  }

  // Destroyed on its own thread. A posted DeferredDelete is still honoured
  // when a QThread finishes, so this holds even when the releaseThread() below
  // is what stops the pool (same reasoning as Gfx/Video/View.cpp).
  if(m_worker)
  {
    m_worker->deleteLater();
    m_worker = nullptr;
  }

  score::ThreadPool::instance().releaseThread();
}

FileWatch& FileWatch::instance()
{
  static std::once_flag init{};
  std::call_once(init, [] { score::AppServices().filewatch.emplace(); });
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
    m_map.emplace(path, watch{.mtime = mtime, .functions{std::move(cb)}});
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
  if(!m_worker)
    return;

  // This executes in the pool thread (m_worker lives there):
  ossia::qt::run_async(m_worker, [this] {
    map_type cur_map;
    {
      std::lock_guard l{m_mtx};
      cur_map = m_map;
    }

    struct pair
    {
      QString path;
      int64_t mtime;
    };
    boost::container::small_vector<pair, 4> vec;

    for(auto& [path, watch] : cur_map)
    {
      if(!QFileInfo::exists(path))
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
  });
}

}
