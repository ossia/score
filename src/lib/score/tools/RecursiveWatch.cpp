#include <score/tools/RecursiveWatch.hpp>
#include <score/tools/ThreadPool.hpp>

#include <QCoreApplication>
#include <QDebug>
#include <QMetaObject>
#include <QPointer>
#include <Qt>

#include <atomic>
#include <cstddef>
#include <mutex>
#include <iostream>
#include <set>

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
static QString to_qstring(const std::filesystem::path& p)
{
#if !defined(_WIN32)
  return QString::fromStdString(p.native());
#else
  return QString::fromStdWString(p.native());
#endif
}

// root is a std::filesystem::path rather than a string so that recursing into a
// symlink keeps the native (wide, on Windows) path llfio handed us: a narrow
// round-trip would be read back as UTF-8 by llfio but as the ANSI code page by
// std::filesystem.
static void for_all_files(
    const std::filesystem::path& root, std::function<void(std::string_view)>& f,
    std::set<std::filesystem::path>& visited)
{
  using namespace LLFIO_V2_NAMESPACE;
  {
    std::error_code ec;
    if(!std::filesystem::is_directory(root, ec) || ec)
      return;

    // llfio never descends into symlinks (nor, on Windows, junctions): we follow
    // them ourselves below, so a link pointing back up the tree must not send us
    // around forever. Windows profiles ship such junctions, e.g.
    // AppData/Local/Application Data -> AppData/Local.
    auto canonical = std::filesystem::canonical(root, ec);
    if(ec || !visited.insert(std::move(canonical)).second)
      return;
  }

  auto pp = path_handle::path(root);
  if(!pp)
    return;

#if defined(_WIN32)
  // llfio gives the entries directly in root as root's own path, in NT form
  // ("\!!\Device\HarddiskVolume3\..."), plus their name, and the deeper ones
  // relative to root: bring both back under root. Joined as-is, the former
  // would come out as "C:\!!\Device\...", which does not exist.
  std::filesystem::path root_nt;
  if(auto cp = pp.value().current_path())
    root_nt = std::move(cp).value();
  const auto under_root = [&](const std::filesystem::path& e) {
    const auto& rn = root_nt.native();
    const auto& en = e.native();
    if(!rn.empty() && en.size() > rn.size() && en.starts_with(rn)
       && (en[rn.size()] == L'\\' || en[rn.size()] == L'/'))
      return root / std::wstring_view{en}.substr(rn.size() + 1);
    return root / e;
  };
#else
  // llfio gives absolute paths, which replace root when joined.
  const auto under_root = [&](const std::filesystem::path& e) { return root / e; };
#endif

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
          case std::filesystem::file_type::symlink:
            for_all_files(under_root(p.first), f, visited);
            break;
          case std::filesystem::file_type::regular: {
            const std::filesystem::path r = under_root(p.first);

#if !defined(_WIN32)
            f(r.native());
#else
            try {
              // UTF-8 whatever the process code page: generic_string() would
              // be in the ANSI one, which only the app's manifest makes UTF-8.
              const auto s = r.generic_u8string();
              f(std::string_view{reinterpret_cast<const char*>(s.data()), s.size()});
            }
            catch(...)
            {
              // Very inefficient but the conversion sometimes throws
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
      qWarning() << "for_all_files: could not enumerate" << to_qstring(root) << ':'
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

void for_all_files(std::string_view root, std::function<void(std::string_view)> f)
try
{
  // root is UTF-8: spell it out, as a narrow string is otherwise read with the
  // ANSI code page on Windows.
  const std::filesystem::path rootp{std::u8string_view{
      reinterpret_cast<const char8_t*>(root.data()), root.size()}};
  std::set<std::filesystem::path> visited;
  for_all_files(rootp, f, visited);
}
catch(...)
{
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
struct RecursiveWatch::AsyncScan
{
  std::atomic_bool cancelled{false};
  std::mutex handler_mutex;
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
  std::shared_ptr<RecursiveWatch::AsyncScan> scan;

  AsyncScanState(
      Map&& w, std::string r, QObject* c, std::shared_ptr<RecursiveWatch::AsyncScan> s)
      : watched{std::move(w)}
      , root{std::move(r)}
      , ctx{c}
      , scan{std::move(s)}
  {
  }

  AsyncScanState(AsyncScanState&& other) noexcept
      : watched{std::move(other.watched)}
      , root{std::move(other.root)}
      , ctx{std::move(other.ctx)}
      , scan{std::move(other.scan)}
  {
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

  // Note that callers should always set a new set of watched things
  // before calling scanAsync.
  cancelAsync();
  m_scan = std::make_shared<AsyncScan>();
  score::TaskPool::instance().post(
      [state = AsyncScanState{std::move(m_asyncWatched), m_root, context, m_scan}] {
    std::vector<std::function<void()>> actions;

    auto send_to_main_thread = [pctx = state.ctx, scan = state.scan, &actions] {
      // Batch-deliver all commit actions to the GUI thread
      QMetaObject::invokeMethod(
          QCoreApplication::instance(),
          [pctx = pctx, scan = scan, actions = std::move(actions)] {
        if(!pctx || scan->cancelled)
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
      std::lock_guard handler_lock{state.scan->handler_mutex};
      if(!state.ctx || state.scan->cancelled)
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

    if(!state.scan->cancelled)
      send_to_main_thread();
  });
}

void RecursiveWatch::cancelAsync() noexcept
{
  if(!m_scan)
    return;
  m_scan->cancelled = true;
  std::lock_guard handler_lock{m_scan->handler_mutex};
  m_scan.reset();
}

namespace
{
struct LiveWatches
{
  std::mutex mutex;
  std::vector<RecursiveWatch*> watches;
};
LiveWatches& liveWatches()
{
  static LiveWatches w;
  return w;
}
}

RecursiveWatch::RecursiveWatch()
{
  auto& live = liveWatches();
  std::lock_guard lock{live.mutex};
  live.watches.push_back(this);
}

RecursiveWatch::~RecursiveWatch()
{
  cancelAsync();
  auto& live = liveWatches();
  std::lock_guard lock{live.mutex};
  std::erase(live.watches, this);
}

void RecursiveWatch::cancelAll() noexcept
{
  auto& live = liveWatches();
  std::lock_guard lock{live.mutex};
  for(auto* w : live.watches)
    w->cancelAsync();
}

void RecursiveWatch::reset()
{
  cancelAsync();
  m_root.clear();
  m_watched.clear();
  m_asyncWatched.clear();
}
}
