#include <score/tools/RecursiveWatch.hpp>
#include <cstddef>
#include <iostream>

#if __has_include(<version>)
#include <version>
#endif

#if __has_include(<llfio.hpp>) \
  && (__cpp_lib_concepts >= 202002L) \
  && (__cpp_lib_span >= 202002L) && __has_include(<span>) \
  && !defined(_WIN32) \
  && !defined(__EMSCRIPTEN__) \
  && (!defined(__clang_major__) || __clang_major__ >= 14)
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
  #define LLFIO_EXPERIMENTAL_STATUS_CODE 1
  #define LLFIO_DISABLE_OPENSSL 1
  #define QUICKCPPLIB_USE_STD_SPAN 1
  #include <llfio.hpp>
  #include <QApplication>
#elif SCORE_HAS_STD_FILESYSTEM
  #include <filesystem>
#elif __has_include(<fts.h>)
  #include <cstring>
  #include <fts.h>
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
  auto pp = path_handle::path(path_view(root, path_view::zero_terminated));
  algorithm::contents_visitor vis;
  vis.contents_include_symlinks = true;
  try
  {
    if(auto res = algorithm::contents(pp.value()))
    {
      for(auto& p : res.value())
      {
        switch(p.second.st_type)
        {
#if !defined(_WIN32)
        case std::filesystem::file_type::symlink:
          for_all_files(p.first.native(), f);
          break;
#endif
        case std::filesystem::file_type::regular:
        {
          std::filesystem::path r{root};
          r /= p.first;

#if !defined(_WIN32)
          f(r.native());
#else
          f(r.generic_string());
#endif
          break;
        }
        default:
          break;
        }
      }
    }
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
  constexpr auto options = fs::directory_options::follow_directory_symlink | fs::directory_options::skip_permission_denied;
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
    catch(...) { continue; }
  }
}
catch(...)
{

}
#elif SCORE_HAS_FTS
static
void process_file(FTS* fts, FTSENT* curr, std::function<void(std::string_view)>& f)
{
  switch (curr->fts_info) {
    case FTS_NS:
    case FTS_DNR:
    case FTS_ERR:
      std::cerr << "for_all_files: " << curr->fts_accpath << ":" << strerror(curr->fts_errno) << std::endl;
      break;

    case FTS_DC:
    case FTS_DOT:
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
    case FTS_NSOK:
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

}

namespace score
{
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
