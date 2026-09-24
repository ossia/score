#include <score/tools/RecursiveWatch.hpp>
#include <score/tools/ThreadPool.hpp>

#include <QCoreApplication>
#include <QDebug>
#include <QMetaObject>
#include <QPointer>
#include <Qt>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <utility>
#include <iostream>

#if __has_include(<version>)
#include <version>
#endif

// https://github.com/ned14/llfio/issues/144
#if __has_include(<llfio.hpp>) && !defined(__EMSCRIPTEN__)
#define SCORE_HAS_LLFIO 1
#elif defined(__APPLE__)
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_10_15
#if __cpp_lib_filesystem >= 201703
#define SCORE_HAS_STD_FILESYSTEM 1
#endif
#endif
#else
#if __cpp_lib_filesystem >= 201703
#define SCORE_HAS_STD_FILESYSTEM 1
#endif
#endif

#if SCORE_HAS_LLFIO
#define LLFIO_HEADERS_ONLY 1
//#define LLFIO_EXPERIMENTAL_STATUS_CODE 1
#define LLFIO_DISABLE_OPENSSL 1
#define QUICKCPPLIB_USE_STD_SPAN 1
// #define OUTCOME_USE_SYSTEM_STATUS_CODE 0

#if defined(__MINGW32__)
#define LLFIO_DISABLE_SIGNAL_GUARD 1
#endif

#if !defined(__has_feature)
#define __has_feature(T) 0
#endif
#if !defined(__has_extension)
#define __has_extension(T) 0
#endif

#include <llfio.hpp>
#elif SCORE_HAS_STD_FILESYSTEM
#include <filesystem>
#elif __has_include(<fts.h>)
#include <fts.h>

#include <cstring>
#define SCORE_HAS_FTS 1
#else
#error Platform missing a simple way to iterate directories.
#endif

namespace score
{
#if SCORE_HAS_LLFIO
void for_all_files(std::string_view root, std::function<void(std::string_view)> f)
{
  using namespace LLFIO_V2_NAMESPACE;
  {
    std::error_code ec;
    const std::filesystem::path rootp{root};
    if(!std::filesystem::is_directory(rootp, ec) || ec)
      return;
  }

  auto pp = path_handle::path(path_view(root, path_view::zero_terminated));
  if(!pp)
    return;
  algorithm::contents_visitor vis;
  vis.contents_include_symlinks = true;
  try
  {
    // permit_racy_reads: on Windows NtQueryDirectoryFile() returns at most ~64Kb of
    // entries per call however large a buffer it is given, so a directory holding more
    // than that (~600 files with short names) cannot be enumerated as an atomic
    // snapshot, and the strict default fails the whole traversal rather than returning
    // the entries. The library is user content: it routinely has such directories, and
    // a best-effort listing of one is worth far more to us than a snapshot of nothing.
    if(auto res = algorithm::contents(
           pp.value(), &vis, 0, false, directory_handle::flags::permit_racy_reads))
    {
      try
      {
      for(auto& p : res.value())
      {
        try {
        switch(p.second.st_type)
        {
#if !defined(_WIN32)
          case std::filesystem::file_type::symlink:
            for_all_files(p.first.native(), f);
            break;
#endif
          case std::filesystem::file_type::regular: {
            std::filesystem::path r{root};
            r /= p.first;

#if !defined(_WIN32)
            f(r.native());
#else
            try {
              f(r.generic_string());
            }
            catch(...)
            {
              // Very inefficient but r.generic_string() sometimes throws
              // filesystem error: in __wide_to_char: Illegal byte sequence
              // on windows
              auto s = QString::fromStdWString(r.native()).toStdString();
              std::replace(s.begin(), s.end(), '\\', '/');
              f(s);
            }
#endif
            break;
          }
          default:
            break;
        }
        }
        catch(const std::exception& e)
        {
        }
        catch(...)
        {
        }
      }
      }
      catch(const std::exception& e)
      {
      }
      catch(...)
      {
      }
    }
    else
    {
      // A failure here yields no files at all, which is indistinguishable from an empty
      // library unless we say so.
      qWarning() << "for_all_files: could not enumerate"
                 << QString::fromUtf8(root.data(), root.size()) << ':'
                 << QString::fromStdString(make_error_code(res.error()).message());
    }
  }
  catch(const std::exception& e)
  {
  }
  catch(...)
  {
  }
}
#endif

#if SCORE_HAS_STD_FILESYSTEM
void for_all_files(std::string_view root, std::function<void(std::string_view)> f)
try
{
  namespace fs = std::filesystem;
  using iterator = fs::recursive_directory_iterator;
#if defined(_WIN32)
  constexpr auto options = fs::directory_options::skip_permission_denied;
#else
  constexpr auto options = fs::directory_options::follow_directory_symlink
                           | fs::directory_options::skip_permission_denied;
#endif
  for(auto it = iterator{root, options}, end = iterator{}; it != end; ++it)
  {
    try
    {
      const auto& path = it->path();
#if defined(_WIN32)
      std::string path_str = path.generic_string();
#else
      std::string_view path_str = path.native();
#endif
      if(path_str.empty())
        continue;
      auto last_slash = path_str.find_last_of('/');
      if(last_slash == path_str.npos || last_slash == path_str.length() - 1)
        continue;
      if(path_str[last_slash + 1] == '.')
      {
        it.disable_recursion_pending();
        continue;
      }

      f(path_str);
    }
    catch(...)
    {
      continue;
    }
  }
}
catch(...)
{
}
#elif SCORE_HAS_FTS
static void
process_file(FTS* fts, FTSENT* curr, std::function<void(std::string_view)>& f)
{
  switch(curr->fts_info)
  {
    case FTS_NS:
    case FTS_DNR:
    case FTS_ERR:
      std::cerr << "for_all_files: " << curr->fts_accpath << ":"
                << strerror(curr->fts_errno) << std::endl;
      break;

    case FTS_DC:
    case FTS_DOT:
      break;

    // Skip unwanted folders (.git, etc)
    case FTS_D: {
      if(curr->fts_name[0] == '.')
      {
        fts_set(fts, curr, FTS_SKIP);
      }

      break;
    }

    // Process our file
    case FTS_NSOK:
    case FTS_F: {
      f(curr->fts_path);
      break;
    }
    case FTS_DP:
    case FTS_SL:
    case FTS_SLNONE:
    case FTS_DEFAULT:
      break;
  }
}

void for_all_files(std::string_view root, std::function<void(std::string_view)> f)
{
  char* files[] = {(char*)root.data(), nullptr};
  auto fts = fts_open(files, FTS_NOCHDIR | FTS_LOGICAL | FTS_NOSTAT, nullptr);
  if(!fts)
  {
    return;
  }

  while(auto curr = fts_read(fts))
  {
    process_file(fts, curr, f);
  }

  fts_close(fts);
}
#endif

}

namespace score
{
// Shared between a RecursiveWatch and the one scan it started: the watch asks
// it to stop, the scan says when it has.
struct RecursiveWatch::ScanControl
{
  std::atomic_bool cancelled{false};
  std::mutex mutex;
  std::condition_variable cv;
  bool done{false};

  void finish()
  {
    {
      std::lock_guard _{mutex};
      done = true;
    }
    cv.notify_all();
  }

  void cancelAndWait()
  {
    cancelled.store(true, std::memory_order_release);
    std::unique_lock l{mutex};
    // Bounded: a pool that never runs the task (shutting down) must not hang
    // the caller; the state below still reports done when the task is dropped.
    if(!cv.wait_for(l, std::chrono::seconds(30), [this] { return done; }))
      qWarning() << "RecursiveWatch: an asynchronous scan did not stop within 30 s";
  }
};

namespace
{
// Mitigation for same bug as https://github.com/microsoft/STL/issues/165
struct AsyncScanState
{
  using Map = ossia::string_map<std::vector<RecursiveWatch::AsyncCallbacks>>;

  Map watched;
  std::string root;
  QPointer<QObject> ctx;
  std::shared_ptr<RecursiveWatch::ScanControl> control;

  AsyncScanState(
      Map&& w, std::string r, QObject* c,
      std::shared_ptr<RecursiveWatch::ScanControl> ctl)
      : watched{std::move(w)}
      , root{std::move(r)}
      , ctx{c}
      , control{std::move(ctl)}
  {
  }

  AsyncScanState(AsyncScanState&& other) noexcept
      : watched{std::move(other.watched)}
      , root{std::move(other.root)}
      , ctx{std::move(other.ctx)}
      , control{std::move(other.control)}
  {
  }

  // Done once the scan has run, or once the pool drops it without running it.
  ~AsyncScanState()
  {
    if(control)
      control->finish();
  }

  bool cancelled() const noexcept
  {
    return control && control->cancelled.load(std::memory_order_acquire);
  }

  AsyncScanState(const AsyncScanState&) = delete;
  AsyncScanState& operator=(AsyncScanState&&) = delete;
  AsyncScanState& operator=(const AsyncScanState&) = delete;
};
}

void RecursiveWatch::scan() const
{
#if !defined(SCORE_DEPLOYMENT_BUILD)
  static const bool disable_library = qEnvironmentVariableIsSet("SCORE_DISABLE_LIBRARY");
  if(Q_UNLIKELY(disable_library))
    return;
#endif

  for_all_files(m_root, [this](std::string_view path) {
    if(path.empty())
      return;
    if(auto last_dot = path.find_last_of('.'); last_dot < path.size() - 1)
    {
      std::string_view suffix = path.substr(last_dot + 1);
      if(auto it = m_watched.find(suffix); it != m_watched.end())
      {
        for(auto& handler : it->second)
        {
          handler.added(path);
        }
      }
    }
  });
}

void RecursiveWatch::scanAsync(QObject* context)
{
#if !defined(SCORE_DEPLOYMENT_BUILD)
  static const bool disable_library = qEnvironmentVariableIsSet("SCORE_DISABLE_LIBRARY");
  if(Q_UNLIKELY(disable_library))
    return;
#endif

  // A scan still running for the previous set of callbacks has nothing left to
  // report: tell it to stop, without waiting for it.
  if(m_scan)
    m_scan->cancelled.store(true, std::memory_order_release);
  m_scan = std::make_shared<ScanControl>();

  // Note that callers should always set a new set of watched things
  // before calling scanAsync.
  score::TaskPool::instance().post(
      [state
       = AsyncScanState{std::move(m_asyncWatched), m_root, context, m_scan}] {
    // Report completion when the body ends, even by an exception: the pool
    // keeps this task object alive until it dequeues the next one, so the
    // state's destructor alone would tell cancel() far too late.
    struct Finished
    {
      const AsyncScanState& state;
      ~Finished()
      {
        if(state.control)
          state.control->finish();
      }
    } finished{state};

    std::vector<std::function<void()>> actions;

    auto send_to_main_thread = [pctx = state.ctx, &actions] {
      // Batch-deliver all commit actions to the GUI thread
      QMetaObject::invokeMethod(
          QCoreApplication::instance(), [pctx = pctx, actions = std::move(actions)] {
        if(!pctx)
          return;
        for(auto& action : actions)
          action();
      }, Qt::QueuedConnection);

      actions.clear();
    };

    for_all_files(state.root, [&](std::string_view path) {
      // The handlers were handed over by the object that asked for the scan and
      // are free to reach back into it. Once it is gone there is nothing left
      // for them to reach, and the walk has nobody to report to either.
      if(state.cancelled() || !state.ctx)
        return;
      if(path.empty())
        return;
      auto last_dot = path.find_last_of('.');
      if(last_dot >= path.size() - 1)
        return;

      std::string_view suffix = path.substr(last_dot + 1);
      auto it = state.watched.find(suffix);
      if(it == state.watched.end())
        return;
      for(auto& handler : it->second)
      {
        if(auto action = handler.filter(path))
        {
          actions.push_back(std::move(action));
          if(actions.size() >= 255)
            send_to_main_thread();
        }
      }
    });

    send_to_main_thread();
  });
}

void RecursiveWatch::cancel()
{
  if(auto scan = std::exchange(m_scan, {}))
    scan->cancelAndWait();
}

RecursiveWatch::~RecursiveWatch()
{
  cancel();
}

void RecursiveWatch::reset()
{
  m_root.clear();
  m_watched.clear();
  m_asyncWatched.clear();
}
}
