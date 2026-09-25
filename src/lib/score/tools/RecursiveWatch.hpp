#pragma once
#include <ossia/detail/string_map.hpp>

#include <QDebug>
#include <QDir>
#include <QString>

#include <score_lib_base_export.h>
#include <smallfun.hpp>

#include <functional>
#include <memory>
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
    using Filter = smallfun::function<
        std::function<void()>(std::string_view),
#if defined(_MSC_VER) && !defined(NDEBUG)
        128,
#else
        64,
#endif
        smallfun::DefaultAlign, smallfun::Methods::Move>;

    Filter filter;
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

  /// Stops the asynchronous scan in flight, if any, and waits until its worker
  /// no longer calls any AsyncCallbacks::filter. The filters are free to reach
  /// into objects their owner is about to destroy, so an owner calls this
  /// before destroying them. A scan started later is unaffected.
  void cancel();

  /// cancel() on every RecursiveWatch alive. Called before the plug-ins and
  /// interfaces the filters reach are destroyed, as not every watch owner is.
  static void cancelAll();

  void reset();

  RecursiveWatch();
  RecursiveWatch(const RecursiveWatch&) = delete;
  RecursiveWatch& operator=(const RecursiveWatch&) = delete;
  ~RecursiveWatch();

  struct ScanControl;

private:
  std::shared_ptr<ScanControl> m_scan;
  std::string m_root;
  ossia::string_map<std::vector<Callbacks>> m_watched;
  ossia::string_map<std::vector<AsyncCallbacks>> m_asyncWatched;
};

SCORE_LIB_BASE_EXPORT
void for_all_files(std::string_view root, std::function<void(std::string_view)> f);
}
