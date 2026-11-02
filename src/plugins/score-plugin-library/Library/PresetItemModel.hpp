#pragma once
#include <Process/Preset.hpp>
#include <Process/Process.hpp>

#include <QAbstractItemModel>
#include <QPointer>

#include <score_plugin_library_export.h>

#include <verdigris>

#include <memory>
#include <optional>
#include <vector>

namespace score
{
struct GUIApplicationContext;
}

namespace Process
{
class ApplicationPlugin;
}

namespace Library
{
/**
 * The library's presets of one kind of process, in folders by category
 * (Process::PresetTree). Rows refer to the application's preset list by index.
 */
class SCORE_PLUGIN_LIBRARY_EXPORT PresetItemModel final : public QAbstractItemModel
{
  W_OBJECT(PresetItemModel)
public:
  PresetItemModel(const score::GUIApplicationContext& ctx, QObject* parent);
  ~PresetItemModel();

  std::vector<Process::Preset>& presets;

  //! The kind of process whose presets it shows; none: no rows
  void setProcess(std::optional<Process::ProcessIdentifier> id);
  const std::optional<Process::ProcessIdentifier>& process() const noexcept
  {
    return m_process;
  }

  //! The process its presets load into, opened from its preset button: its
  //! own presets are shown too, in a "Built-in" folder, and its current one
  void setTarget(const Process::ProcessModel* proc);
  const Process::ProcessModel* target() const noexcept { return m_target; }
  //! The target's current preset changed
  void refreshCurrent();
  //! The target's current preset, if it is a row
  QModelIndex currentPresetIndex() const;

  //! Only the presets whose name or category contains it
  void setFilterText(const QString& text);
  const QString& filterText() const noexcept { return m_filter; }

  //! The row in `presets` of a preset's index; -1 for a folder or a
  //! built-in preset
  int presetRow(const QModelIndex& index) const;
  const Process::Preset* preset(const QModelIndex& index) const;
  QModelIndex indexOfPreset(int presetRow) const;
  //! The row in `presets` of the preset in this file; -1 if none is
  int presetRowOf(const QString& path) const;
  //! A folder's path, or the folder a preset is in
  QStringList folderPath(const QModelIndex& index) const;
  QModelIndex indexOfFolder(const QStringList& path) const;

  //! Saves the current state of a process as a new preset, in a category
  bool savePreset(const Process::ProcessModel& proc, const QString& category = {});
  //! Only a preset in the user library can be renamed or deleted
  bool canEdit(int presetRow) const;
  //! Moves its file to the trash
  bool removePreset(int presetRow);
  //! Puts a preset in another category ("" for none, "A/B" for a subfolder)
  bool movePreset(int presetRow, const QString& category);
  //! An empty category of the shown process's presets, to move or save some
  //! into; kept until the application quits, or as long as presets are in it
  QModelIndex addCategory(const QStringList& path);
  //! Every category of the library's presets shown, as "A/B" paths
  QStringList categories() const;
  //! Whether it is the folder of the target's own presets, or in it
  bool isBuiltin(const QModelIndex& index) const;

  //! After a rename, where the preset now is
  void renamed(QModelIndex index) E_SIGNAL(SCORE_PLUGIN_LIBRARY_EXPORT, renamed, index)

  QModelIndex index(int row, int column, const QModelIndex& parent) const override;
  QModelIndex parent(const QModelIndex& child) const override;
  int rowCount(const QModelIndex& parent) const override;
  int columnCount(const QModelIndex& parent) const override;
  QVariant data(const QModelIndex& index, int role) const override;
  Qt::ItemFlags flags(const QModelIndex& index) const override;
  bool setData(const QModelIndex& index, const QVariant& value, int role) override;

  bool dropMimeData(
      const QMimeData* data, Qt::DropAction act, int row, int col,
      const QModelIndex& parent) override;
  bool canDropMimeData(
      const QMimeData* data, Qt::DropAction act, int row, int col,
      const QModelIndex& parent) const override;
  QMimeData* mimeData(const QModelIndexList& indexes) const override;
  Qt::DropActions supportedDragActions() const override;
  Qt::DropActions supportedDropActions() const override;

  struct Node;

private:
  bool shows(const Process::Preset& p) const noexcept;
  QString categoryKey() const;
  Node& folder(const QStringList& path);
  void rebuild();
  void build();
  Node* node(const QModelIndex& index) const;
  QModelIndex indexOf(const Node* n) const;

  Process::ApplicationPlugin* m_plugin{};
  std::optional<Process::ProcessIdentifier> m_process;
  QPointer<const Process::ProcessModel> m_target;
  std::vector<Process::Preset> m_builtins;
  QString m_filter;
  std::unique_ptr<Node> m_root;
  //! Whether the preset being removed was one of the rows
  bool m_removingShown{};
  //! Categories created empty, by kind of process (categoryKey)
  std::vector<std::pair<QString, QStringList>> m_emptyCategories;
  //! The preset being dragged out of the view, to move it within
  mutable int m_dragged{-1};
};
}
