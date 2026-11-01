#pragma once
#include <Process/Preset.hpp>

#include <ossia/detail/hash_map.hpp>

#include <QStringList>

#include <score_lib_process_export.h>

#include <memory>
#include <vector>

namespace score
{
struct DocumentContext;
}

namespace Process
{
class ProcessModel;

//! "Foo / Bar//Baz" -> {"Foo", "Bar", "Baz"}: a category as it is nested
SCORE_LIB_PROCESS_EXPORT QStringList presetCategoryPath(const QString& category);

/**
 * The presets of a process, in folders by category, as the library shows
 * them and the preset button's Forward / Back walk them: in each folder its
 * presets, then its subfolders, each by name; the process's own presets (a
 * plug-in's) last, in a "Built-in" folder.
 */
struct SCORE_LIB_PROCESS_EXPORT PresetTree
{
  struct Node
  {
    QString name;
    //! A preset, or the index of a subfolder in `children`, in display order
    struct Entry
    {
      const Process::Preset* preset{};
      int child = -1;
    };
    std::vector<Entry> entries;
    std::vector<std::unique_ptr<Node>> children;
    ossia::hash_map<QString, int> childIndex;
  };
  Node root;

  //! The library's presets of this process, then its built-in ones: both
  //! must outlive the tree
  PresetTree(
      const Process::ProcessModel& proc, const std::vector<Process::Preset>& library,
      const std::vector<Process::Preset>& builtins);

  //! The library's presets of a kind of process, when there is no process
  PresetTree(
      const Process::ProcessIdentifier& id, const std::vector<Process::Preset>& library,
      const std::vector<Process::Preset>& builtins = {});

  //! Depth first: the order of the library's tree
  std::vector<const Process::Preset*> ordered() const;

  static QString builtinFolderName();

private:
  void addLibrary(std::vector<const Process::Preset*> presets);
  void addBuiltins(const std::vector<Process::Preset>& builtins);
  static Node& folder(Node& from, const QString& category);
};

//! The preset last loaded into a process, kept on it for the session so that
//! Forward / Back and the library know where they are. Not saved, not undone.
SCORE_LIB_PROCESS_EXPORT void
rememberPreset(const Process::ProcessModel& proc, const Process::Preset& preset);
SCORE_LIB_PROCESS_EXPORT bool
isCurrentPreset(const Process::ProcessModel& proc, const Process::Preset& preset);
//! {category, name} of the current preset, empty if none: to put it back
SCORE_LIB_PROCESS_EXPORT QStringList presetCursor(const Process::ProcessModel& proc);
SCORE_LIB_PROCESS_EXPORT void
setPresetCursor(const Process::ProcessModel& proc, const QStringList& cursor);

//! Loads a preset into a process as the user does: one undo step, and it
//! becomes the process's current preset
SCORE_LIB_PROCESS_EXPORT bool loadPreset(
    const Process::ProcessModel& proc, const Process::Preset& preset,
    const score::DocumentContext& ctx);
}
