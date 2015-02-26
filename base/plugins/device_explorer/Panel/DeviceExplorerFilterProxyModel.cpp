#include "DeviceExplorerFilterProxyModel.hpp"


DeviceExplorerFilterProxyModel::DeviceExplorerFilterProxyModel(QObject* parent)
    : QSortFilterProxyModel(parent),
      m_col(0)
{

}

void
DeviceExplorerFilterProxyModel::setColumn(int col)
{
    m_col = col;
}

/*
  Return true if the item must be included in the model.
*/
bool
DeviceExplorerFilterProxyModel::filterAcceptsRow(int srcRow,
        const QModelIndex& srcParent) const
{
    //inspired from http://qt-project.org/forums/viewthread/7782/
    //The filter must accept rows that match themselves, that have a parent that matches (on its own),  or that have a child that matches. That is, if a parent node matches, it will show all its children, even if they themselves do not match, but if a a child node matches, it will show the parent, but not its (non-matching) siblings.

    if(filterAcceptsRowItself(srcRow, srcParent))
    {
        return true;
    }

    //Accept if any of the parents is accepted on its own
    for(QModelIndex parent = srcParent; parent.isValid(); parent = parent.parent())
        if(filterAcceptsRowItself(parent.row(), parent.parent()))
        {
            return true;
        }

    //Accept if any of the children is accepted on its own
    if(hasAcceptedChildren(srcRow, srcParent))
    {
        return true;
    }

    return false;
}

bool
DeviceExplorerFilterProxyModel::filterAcceptsRowItself(int srcRow, const QModelIndex& srcParent) const
{
    #if 1
    const int col = m_col;
    QModelIndex index = sourceModel()->index(srcRow, col, srcParent);
    return sourceModel()->data(index).toString().contains(filterRegExp());
    #else
    return QSortFilterProxyModel::filterAcceptsRow(srcRow, srcParent);  //which col ???
    #endif
}

bool
DeviceExplorerFilterProxyModel::hasAcceptedChildren(int srcRow, const QModelIndex& srcParent) const
{
    const int col = m_col;
    QModelIndex index = sourceModel()->index(srcRow, col, srcParent);

    if(! index.isValid())
    {
        return false;
    }

    Q_ASSERT(index.model());
    const int childCount = index.model()->rowCount(index);

    if(childCount == 0)
    {
        return false;
    }

    for(int i = 0; i < childCount; ++i)
    {
        if(filterAcceptsRowItself(i, index))
        {
            return true;
        }

        //recursive call : depth-first search
        if(hasAcceptedChildren(i, index))
        {
            return true;
        }

    }

    return false;
}
