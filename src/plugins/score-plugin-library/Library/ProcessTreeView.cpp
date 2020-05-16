#include "ProcessTreeView.hpp"
#include <QSortFilterProxyModel>
#include <QDrag>
#include <QMimeData>
#include <QGuiApplication>
#include <wobjectimpl.h>
#include <score/widgets/Pixmap.hpp>

W_OBJECT_IMPL(Library::ProcessTreeView)
namespace Library
{
void ProcessTreeView::selectionChanged(
    const QItemSelection& sel,
    const QItemSelection& desel)
{
  setDropIndicatorShown(true);

  if (sel.size() > 0)
  {
    auto idx = sel.indexes().front();
    auto proxy = (QSortFilterProxyModel*)this->model();
    auto model_idx = proxy->mapToSource(idx);
    auto data = reinterpret_cast<TreeNode<ProcessData>*>(
        model_idx.internalPointer());

    selected(*data);
  }
  else if (desel.size() > 0)
  {
    selected({});
  }
}

QModelIndexList ProcessTreeView::selectedDraggableIndexes() const
{
  QModelIndexList indexes = selectedIndexes();
  auto m = QTreeView::model();
  auto isNotDragEnabled = [m](const QModelIndex& index) {
    return !(m->flags(index) & Qt::ItemIsDragEnabled);
  };
  indexes.erase(
      std::remove_if(indexes.begin(), indexes.end(), isNotDragEnabled),
      indexes.end());
  return indexes;
}

void ProcessTreeView::startDrag(Qt::DropActions)
{
  QModelIndexList indexes = selectedDraggableIndexes();
  if (indexes.count() == 1)
  {
   // auto proxy = (QSortFilterProxyModel*)this->model();
   // auto model_idx = proxy->mapFromSource(indexes.first());

    //QMimeData* data = proxy->mimeData(QModelIndexList{model_idx});
    QMimeData* data = QTreeView::model()->mimeData(indexes);
    if (!data)
      return;

    QDrag* drag = new QDrag(this);
    drag->setMimeData(data);
    /*
    auto p = score::get_pixmap(QStringLiteral(":/icons/cursor_process_audio.png"));
    drag->setDragCursor(p, Qt::CopyAction);
    drag->setDragCursor(p, Qt::MoveAction);
    */
    drag->exec();
  }
}

}
