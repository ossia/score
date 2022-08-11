#pragma once
#include <ossia/detail/string_map.hpp>

#include <QDebug>
#include <QDir>
#include <QString>

#include <score_lib_base_export.h>

#include <functional>
#include <vector>

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

  void setWatchedFolder(std::string root) { m_root = root; }

  void registerWatch(std::string extension, Callbacks callbacks)
  {
    m_watched[extension].push_back(callbacks);
  }

  void scan() const;

  void reset()
  {
    m_root.clear();
    m_watched.clear();
  }

private:
  std::string m_root;
  ossia::string_map<std::vector<Callbacks>> m_watched;
};
}
