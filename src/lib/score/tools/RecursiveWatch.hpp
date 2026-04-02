#pragma once
#include <ossia/detail/string_map.hpp>

#include <QDebug>
#include <QDir>
#include <QString>

#include <score_lib_base_export.h>

#include <functional>
#include <vector>

class QObject;

namespace score
{
class SCORE_LIB_BASE_EXPORT RecursiveWatch
{
public:
  struct Callbacks
  {
    std::function<void(std::string_view)> added;
    std::function<void(std::string_view)> removed;
  };

  struct Watched
  {
    std::string ext;
    Callbacks callbacks;
  };

  /// Async two-phase callbacks for off-main-thread scanning.
  /// \a filter is called on a worker thread for each file matching the extension.
  /// It should return a non-empty std::function<void()> (a "commit action") to
  /// accept the file; that action will be invoked on the GUI thread.
  /// Return an empty std::function (or {}) to reject the file.
  struct AsyncCallbacks
  {
    std::function<std::function<void()>(std::string_view path)> filter;
  };

  void setWatchedFolder(std::string root) { m_root = root; }

  void registerWatch(std::string extension, Callbacks callbacks)
  {
    m_watched[extension].push_back(std::move(callbacks));
  }

  void registerWatch(std::string extension, AsyncCallbacks callbacks)
  {
    m_asyncWatched[extension].push_back(std::move(callbacks));
  }

  /// Synchronous scan: calls Callbacks::added on the calling thread.
  void scan() const;

  /// Asynchronous scan: runs directory traversal + AsyncCallbacks::filter
  /// on a worker thread, then batch-delivers the returned commit actions
  /// on the GUI thread via \a context's event loop.
  void scanAsync(QObject* context);

  void reset()
  {
    m_root.clear();
    m_watched.clear();
    m_asyncWatched.clear();
  }

private:
  std::string m_root;
  ossia::string_map<std::vector<Callbacks>> m_watched;
  ossia::string_map<std::vector<AsyncCallbacks>> m_asyncWatched;
};

SCORE_LIB_BASE_EXPORT
void for_all_files(std::string_view root, std::function<void(std::string_view)> f);
}
