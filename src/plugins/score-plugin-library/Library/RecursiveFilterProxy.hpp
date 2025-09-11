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

  const QString& pattern() const noexcept;
  void setPattern(const QString& p);

protected:
  QString m_textPattern;

  int columnCount(const QModelIndex& parent = QModelIndex()) const override;
  bool filterAcceptsRow(int srcRow, const QModelIndex& srcParent) const override;
  virtual bool filterAcceptsRowItself(int srcRow, const QModelIndex& srcParent) const
      = 0;
  bool hasAcceptedChildren(int srcRow, const QModelIndex& srcParent) const;
};

class ProcessFilterProxy : public RecursiveFilterProxy
{
public:
  using RecursiveFilterProxy::RecursiveFilterProxy;
  bool filterAcceptsRowItself(int srcRow, const QModelIndex& srcParent) const final;
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
  {
    return static_cast<QFileSystemModel*>(QSortFilterProxyModel::sourceModel());
  }

  bool filterAcceptsRowItself(int srcRow, const QModelIndex& srcParent) const override
  {
    QModelIndex index = sourceModel()->index(srcRow, 0, srcParent);
    const QVariant& data = sourceModel()->data(index);

    return data.toString().contains(m_textPattern, Qt::CaseInsensitive);
  }

  bool filterAcceptsRow(int srcRow, const QModelIndex& srcParent) const override
  {
    const QModelIndex index = sourceModel()->index(srcRow, 0, srcParent);
    if(!isChildOfRoot(index))
    {
      return false;
    }

    if(filterAcceptsRowItself(srcRow, srcParent))
    {
      return true;
    }

    // Accept if any of the parents is accepted on its own
    for(QModelIndex parent = srcParent; parent.isValid(); parent = parent.parent())
    {
      // We went up to the root. We know that we are a child due to the startsWith.
      if(parent == fixedRootIndex)
        return false;

      if(filterAcceptsRowItself(parent.row(), parent.parent()))
      {
        return true;
      }
    }

    // Accept if any of the children is accepted on its own
    return hasAcceptedChildren(srcRow, srcParent);
  }
};
}
