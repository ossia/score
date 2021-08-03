#include <score/tools/RecursiveWatch.hpp>
#include <cstddef>

#if defined(__APPLE__)
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

#if SCORE_HAS_STD_FILESYSTEM
#include <filesystem>
#elif __has_include(<fts.h>)
#include <fts.h>
#define SCORE_HAS_FTS 1
#else
#error Platform missing a simple way to iterate directories.
#endif

namespace score
{
#if SCORE_HAS_STD_FILESYSTEM
void for_all_files(std::string_view root, std::function<void(std::string_view)> f)
{
  namespace fs = std::filesystem;
  using iterator = fs::recursive_directory_iterator;
  for(auto it = iterator{root}; it != iterator{}; ++it)
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
}
#elif SCORE_HAS_FTS
static
void process_file(FTS* fts, FTSENT* curr, std::function<void(std::string_view)>& f)
{
  switch (curr->fts_info) {
    case FTS_NS:
    case FTS_DNR:
    case FTS_ERR:
      qDebug() << "for_all_files: " << curr->fts_accpath << strerror(curr->fts_errno);
      break;

    case FTS_DC:
    case FTS_DOT:
    case FTS_NSOK:
      break;

    // Skip unwanted folders (.git, etc)
    case FTS_D:
    {
      if(curr->fts_name[0] == '.')
      {
        fts_set(fts, curr, FTS_SKIP);
      }

      break;
    }

    // Process our file
    case FTS_F:
    {
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
  char *files[] = { (char *) root.data(), nullptr };
  auto fts = fts_open(files, FTS_NOCHDIR | FTS_LOGICAL | FTS_NOSTAT, nullptr);
  if (!fts) {
    return;
  }

  while (auto curr = fts_read(fts)) {
    process_file(fts, curr, f);
  }

  fts_close(fts);
}
#endif

void RecursiveWatch::scan() const
{
  for_all_files(m_root, [this] (std::string_view path) {
    if(path.empty())
      return;
    if(auto last_dot = path.find_last_of('.'); last_dot < path.size() - 1) {
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

}
