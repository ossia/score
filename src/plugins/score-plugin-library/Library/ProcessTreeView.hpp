#pragma once
#include <Library/ProcessesItemModel.hpp>

#include <score/tools/std/Optional.hpp>

#include <QTreeView>

#include <verdigris>

#include <score_plugin_library_export.h>
namespace Library
{
class SCORE_PLUGIN_LIBRARY_EXPORT ProcessTreeView : public QTreeView
{
  W_OBJECT(ProcessTreeView)
public:
  using QTreeView::QTreeView;

  void selected(std::optional<Library::ProcessData> p) E_SIGNAL(SCORE_PLUGIN_LIBRARY_EXPORT, selected, p);
  void doubleClicked(Library::ProcessData p) E_SIGNAL(SCORE_PLUGIN_LIBRARY_EXPORT, doubleClicked, p);

private:
  QModelIndexList selectedDraggableIndexes() const;
  void selectionChanged(const QItemSelection& selected, const QItemSelection& deselected) override;
  void startDrag(Qt::DropActions supportedActions) override;
  void mouseDoubleClickEvent(QMouseEvent *event) override;
  Library::ProcessData* dataFromViewIndex(QModelIndex);
};
}
