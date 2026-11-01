#pragma once
#include <Process/Preset.hpp>

#include <QListView>

#include <score_plugin_library_export.h>

#include <verdigris>

namespace Library
{
class SCORE_PLUGIN_LIBRARY_EXPORT PresetListView final : public QListView
{
  W_OBJECT(PresetListView)
public:
  using QListView::QListView;
  ~PresetListView();
  void mouseDoubleClickEvent(QMouseEvent* event) override;
  void paintEvent(QPaintEvent* e) override;
  void contextMenuEvent(QContextMenuEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;

  void selected(std::optional<Process::Preset> p)
      E_SIGNAL(SCORE_PLUGIN_LIBRARY_EXPORT, selected, p)
  void doubleClicked(Process::Preset p)
      E_SIGNAL(SCORE_PLUGIN_LIBRARY_EXPORT, doubleClicked, p)

private:
  //! The preset's row in the library's list, or -1
  int presetRow(const QModelIndex& proxy_index) const;
};

}
