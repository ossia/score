#include "ProcessTreeView.hpp"
#include <QSortFilterProxyModel>
#include <wobjectimpl.h>

W_OBJECT_IMPL(Library::ProcessTreeView)
namespace Library
{
void ProcessTreeView::selectionChanged(
    const QItemSelection& sel,
    const QItemSelection& desel)
{
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
}
