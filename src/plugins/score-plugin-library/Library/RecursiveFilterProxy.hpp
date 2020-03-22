#pragma once
#include <QSortFilterProxyModel>


namespace Library
{
class RecursiveFilterProxy final : public QSortFilterProxyModel
{
public:
  using QSortFilterProxyModel::QSortFilterProxyModel;

private:
  bool
  filterAcceptsRow(int srcRow, const QModelIndex& srcParent) const override
  {
    if (filterAcceptsRowItself(srcRow, srcParent))
    {
      return true;
    }

    // Accept if any of the parents is accepted on its own
    for (QModelIndex parent = srcParent; parent.isValid();
         parent = parent.parent())
      if (filterAcceptsRowItself(parent.row(), parent.parent()))
      {
        return true;
      }

    // Accept if any of the children is accepted on its own
    return hasAcceptedChildren(srcRow, srcParent);
  }

  bool filterAcceptsRowItself(int srcRow, const QModelIndex& srcParent) const
  {
    QModelIndex index = sourceModel()->index(srcRow, 0, srcParent);
    return sourceModel()->data(index).toString().contains(filterRegExp());
  }

  bool hasAcceptedChildren(int srcRow, const QModelIndex& srcParent) const
  {
    QModelIndex index = sourceModel()->index(srcRow, 0, srcParent);

    if (!index.isValid())
      return false;

    SCORE_ASSERT(index.model());
    const int childCount = index.model()->rowCount(index);

    if (childCount == 0)
      return false;

    for (int i = 0; i < childCount; ++i)
    {
      if (filterAcceptsRowItself(i, index))
        return true;

      if (hasAcceptedChildren(i, index))
        return true;
    }

    return false;
  }
};
}
