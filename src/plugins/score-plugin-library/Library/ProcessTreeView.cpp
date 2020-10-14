#include "ProcessTreeView.hpp"

#include <score/widgets/Pixmap.hpp>

#include <QDrag>
#include <QMouseEvent>
#include <QGuiApplication>
#include <QMimeData>
#include <QSortFilterProxyModel>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Library::ProcessTreeView)
namespace Library
{
Library::ProcessData* ProcessTreeView::dataFromViewIndex(QModelIndex idx)
{
  SCORE_ASSERT(idx.isValid());
  auto proxy = (QSortFilterProxyModel*)this->model();
  auto model_idx = proxy->mapToSource(idx);
  auto data = reinterpret_cast<TreeNode<ProcessData>*>(model_idx.internalPointer());
  return data;
}

void ProcessTreeView::selectionChanged(const QItemSelection& sel, const QItemSelection& desel)
{
  setDropIndicatorShown(true);

  if (!sel.isEmpty())
  {
    selected(*dataFromViewIndex(sel.indexes().front()));
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
  auto isNotDragEnabled
      = [m](const QModelIndex& index) { return !(m->flags(index) & Qt::ItemIsDragEnabled); };
  indexes.erase(std::remove_if(indexes.begin(), indexes.end(), isNotDragEnabled), indexes.end());
  return indexes;
}

void ProcessTreeView::startDrag(Qt::DropActions)
{
  QModelIndexList indexes = selectedDraggableIndexes();
  if (indexes.count() == 1)
  {
    // auto proxy = (QSortFilterProxyModel*)this->model();
    // auto model_idx = proxy->mapFromSource(indexes.first());

    // QMimeData* data = proxy->mimeData(QModelIndexList{model_idx});
    QMimeData* data = QTreeView::model()->mimeData(indexes);
    if (!data)
      return;

    QDrag* drag = new QDrag(this);
    drag->setMimeData(data);
    /*
    auto p =
    score::get_pixmap(QStringLiteral(":/icons/cursor_process_audio.png"));
    drag->setDragCursor(p, Qt::CopyAction);
    drag->setDragCursor(p, Qt::MoveAction);
    */
    drag->exec();
  }
}

void ProcessTreeView::mouseDoubleClickEvent(QMouseEvent *event)
{
  auto index = indexAt(event->pos());
  if(index.isValid())
  {
    auto data = dataFromViewIndex(index);
    if(data->key != UuidKey<Process::ProcessModel>{})
    {
      doubleClicked(*data);
      event->accept();
      return;
    }
  }
  QTreeView::mouseDoubleClickEvent(event);
}

}
