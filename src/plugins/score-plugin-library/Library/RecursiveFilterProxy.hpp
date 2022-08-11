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

  const QString& pattern() const noexcept
  { return m_textPattern; }
  void setPattern(const QString& p)
  {
    beginResetModel();
    auto old = m_textPattern;
    m_textPattern = p;
    endResetModel();
    // if(!p.contains(old))
    //  invalidateFilter();
  }

protected:
  QString m_textPattern;
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

    return data.toString().contains(m_textPattern, Qt::CaseInsensitive);
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

  QModelIndex fixedRootIndex{};
private:

  bool isChildOfRoot(const QModelIndex& m) const noexcept
  {
    if(!m.isValid())
    {
      return false;
    }
    if(m == fixedRootIndex)
      return true;
    if(auto parent = m.parent(); parent == fixedRootIndex)
    {
      return true;
    }
    else
    {
      return isChildOfRoot(parent);
    }
  }

  QFileSystemModel* sourceModel() const noexcept
  { return static_cast<QFileSystemModel*>(QSortFilterProxyModel::sourceModel());}
  bool
  filterAcceptsRow(int srcRow, const QModelIndex& srcParent) const override
  {
    const QModelIndex index = sourceModel()->index(srcRow, 0, srcParent);
    if(!isChildOfRoot(index))
    {
      return false;
    }

    if (filterAcceptsRowItself(srcRow, srcParent))
    {
      return true;
    }

    // Accept if any of the parents is accepted on its own
    for (QModelIndex parent = srcParent; parent.isValid();
         parent = parent.parent())
    {
      // We went up to the root. We know that we are a child due to the startsWith.
      if (parent == fixedRootIndex)
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
