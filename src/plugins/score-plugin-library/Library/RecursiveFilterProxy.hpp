#pragma once
#include <score/tools/Debug.hpp>

#include <QFileSystemModel>
#include <QSortFilterProxyModel>

namespace Library
{
class RecursiveFilterProxy : public QSortFilterProxyModel
{
public:
  using QSortFilterProxyModel::QSortFilterProxyModel;

protected:
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
    const QVariant& data = sourceModel()->data(index);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    return data.toString().contains(filterRegExp());
#else
    return data.toString().contains(
        filterRegularExpression());
#endif
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

class FileSystemRecursiveFilterProxy final : public RecursiveFilterProxy
{
public:
  using RecursiveFilterProxy::RecursiveFilterProxy;

  QString fixedRoot{};
private:
  QFileSystemModel* sourceModel() const noexcept
  { return static_cast<QFileSystemModel*>(QSortFilterProxyModel::sourceModel());}
  bool
  filterAcceptsRow(int srcRow, const QModelIndex& srcParent) const override
  {
    QModelIndex index = sourceModel()->index(srcRow, 0, srcParent);
    if(!sourceModel()->filePath(index).startsWith(fixedRoot))
    {
      return false;
    }

    if (filterAcceptsRowItself(srcRow, srcParent))
    {
      return true;
    }

    QModelIndex root_index = sourceModel()->index(fixedRoot);
    // Accept if any of the parents is accepted on its own
    for (QModelIndex parent = srcParent; parent.isValid();
         parent = parent.parent())
    {
      // We went up to the root. We know that we are a child due to the startsWith.
      if (parent == root_index)
        return false;

      if (filterAcceptsRowItself(parent.row(), parent.parent()))
      {
        return true;
      }
    }

    // Accept if any of the children is accepted on its own
    return hasAcceptedChildren(srcRow, srcParent);
  }
};
}
