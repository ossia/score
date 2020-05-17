#pragma once
#include <Library/ProcessesItemModel.hpp>

#include <score/tools/std/Optional.hpp>

#include <QTreeView>

#include <verdigris>

namespace Library
{
class ProcessTreeView : public QTreeView
{
  W_OBJECT(ProcessTreeView)
public:
  using QTreeView::QTreeView;
  void selected(optional<Library::ProcessData> p) W_SIGNAL(selected, p);

  void selectionChanged(const QItemSelection& selected, const QItemSelection& deselected) override;

private:
  QModelIndexList selectedDraggableIndexes() const;
  void startDrag(Qt::DropActions supportedActions) override;
};
}
