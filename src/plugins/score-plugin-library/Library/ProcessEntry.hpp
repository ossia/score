#pragma once
#include <Process/ProcessFactory.hpp>
#include <Process/ProcessMimeSerialization.hpp>

#include <score/tools/File.hpp>

#include <QIcon>
#include <QStringList>

#include <memory>
#include <mutex>
#include <optional>
#include <vector>

#include <score_plugin_library_export.h>

namespace score
{
struct ApplicationContext;
}

namespace Library
{
struct ProcessData : Process::ProcessData
{
  QIcon icon;
};

//! A subtree of library entries. Pure data, safe to produce on any thread;
//! only ProcessesItemModel::publish/replaceChildren turn it into tree nodes.
struct StagedNode
{
  ProcessData data;
  std::vector<StagedNode> children;
};

//! One scan result: where to attach (the process node identified by rootKey,
//! then a chain of category folders) and what to attach.
struct ProcessEntry
{
  Process::ProcessModelFactory::ConcreteKey rootKey;
  QStringList categoryPath;
  StagedNode node;

  //! Attach under the root of the tree rather than under a process node,
  //! ignoring rootKey. What another machine offers is named by categories
  //! that exist nowhere here, so there is no local process to anchor to.
  //! A default rootKey does NOT mean this: it means a key that resolved to
  //! nothing, and such an entry is still dropped.
  bool atRoot{};
};

//! The category a scanned file lands in: its parent folder, except for files
//! directly at the packages root or in the process's default preset folder,
//! which land directly under the process node. Pure, usable from any thread.
//! presetsPath is "Presets/<process name>".
inline QStringList categoryPathForFile(
    const score::PathInfo& file, std::string_view packagesRoot,
    std::string_view presetsPath) noexcept
{
  if(file.absolutePath == packagesRoot || file.absolutePath.ends_with(presetsPath))
    return {};
  return {QString::fromUtf8(file.parentDirName.data(), file.parentDirName.size())};
}

//! The two paths categoryPathForFile needs, captured at setup() time and read
//! from scan workers. setup() may run again (library rescan) while a worker
//! from the previous scan is still filtering, hence the locked swap (a plain
//! mutex: std::atomic<std::shared_ptr> is not implemented everywhere yet).
class SCORE_PLUGIN_LIBRARY_EXPORT CategoryPaths
{
public:
  struct Paths
  {
    std::string packagesRoot;
    std::string presets;
  };

  //! GUI thread (setup). processName is the process's pretty name.
  void init(std::string processName, const score::ApplicationContext& ctx);

  //! Worker thread: category path for a scanned file.
  QStringList operator()(const score::PathInfo& file) const noexcept
  {
    if(auto p = get())
      return categoryPathForFile(file, p->packagesRoot, p->presets);
    return {};
  }

  //! Worker thread: the raw paths, for handlers with custom category logic.
  std::shared_ptr<const Paths> get() const noexcept
  {
    std::lock_guard lock{m_mutex};
    return m_paths;
  }

private:
  mutable std::mutex m_mutex;
  std::shared_ptr<const Paths> m_paths;
};
}
