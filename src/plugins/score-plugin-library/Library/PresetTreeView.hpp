#pragma once
#include <Process/Preset.hpp>

#include <ossia/detail/hash_map.hpp>

#include <QSet>
#include <QTreeView>

#include <score_plugin_library_export.h>

#include <verdigris>

namespace Library
{
class PresetItemModel;

//! The library's presets of one kind of process, in folders by category
class SCORE_PLUGIN_LIBRARY_EXPORT PresetTreeView final : public QTreeView
{
  W_OBJECT(PresetTreeView)
public:
  explicit PresetTreeView(QWidget* parent = nullptr);
  ~PresetTreeView();

  void setPresetModel(PresetItemModel* model);
  PresetItemModel* presetModel() const noexcept { return m_model; }

  //! The current item became this preset, or a folder / nothing
  void selected(std::optional<Process::Preset> p)
      E_SIGNAL(SCORE_PLUGIN_LIBRARY_EXPORT, selected, p)
  //! Without a target process: create a process from it
  void doubleClicked(Process::Preset p)
      E_SIGNAL(SCORE_PLUGIN_LIBRARY_EXPORT, doubleClicked, p)
  //! With one: load it into it (double-click, Enter)
  void loadRequested(Process::Preset p)
      E_SIGNAL(SCORE_PLUGIN_LIBRARY_EXPORT, loadRequested, p)
  //! Escape: back to the preset before the ones auditioned
  void cancelRequested() E_SIGNAL(SCORE_PLUGIN_LIBRARY_EXPORT, cancelRequested)

  //! Makes the target's current preset the current item, opening its folder
  void showCurrentPreset();
  //! Asks for the name of a new category, in the folder `in`
  void newCategory(const QStringList& in);

protected:
  void mouseDoubleClickEvent(QMouseEvent* event) override;
  void paintEvent(QPaintEvent* e) override;
  void contextMenuEvent(QContextMenuEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;
  void currentChanged(const QModelIndex& current, const QModelIndex& previous) override;
  void focusInEvent(QFocusEvent* event) override;

private:
  QString expansionKey() const;
  void saveState();
  void restoreState();
  void scrollToCurrentLater();

  PresetItemModel* m_model{};
  //! The folders open for each kind of process, for the session
  ossia::hash_map<QString, QSet<QString>> m_expanded;
  //! Across a rebuild of the model: the current preset's file, or its name
  QString m_currentPath, m_currentName, m_currentCategory;
  bool m_restoring{};
};
}
