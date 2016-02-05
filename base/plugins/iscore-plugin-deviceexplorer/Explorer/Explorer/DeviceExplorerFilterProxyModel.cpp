#include <QAbstractItemModel>
#include <QString>
#include <QVariant>
#include <iscore/tools/Todo.hpp>
#include "DeviceExplorerFilterProxyModel.hpp"
#include <Explorer/Explorer/DeviceExplorerModel.hpp>
class QObject;

namespace Explorer
{
DeviceExplorerFilterProxyModel::DeviceExplorerFilterProxyModel(QObject* parent)
    : QSortFilterProxyModel(parent),
      m_col(Explorer::Column::Name)
{

}

void
DeviceExplorerFilterProxyModel::setColumn(
        Explorer::Column col)
{
    m_col = col;
    invalidate();
}

/*
  Return true if the item must be included in the model.
*/
bool
DeviceExplorerFilterProxyModel::filterAcceptsRow(
        int srcRow,
        const QModelIndex& srcParent) const
{
    // inspired from http://qt-project.org/forums/viewthread/7782/
    // The filter must accept rows that match themselves,
    // that have a parent that matches (on its own),
    // or that have a child that matches.
    // That is, if a parent node matches, it will show all its children,
    // even if they themselves do not match, but if a a child node matches,
    // it will show the parent, but not its (non-matching) siblings.

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
    return hasAcceptedChildren(srcRow, srcParent);
}

bool
DeviceExplorerFilterProxyModel::filterAcceptsRowItself(
        int srcRow,
        const QModelIndex& srcParent) const
{
    static const QMap<QString, bool> values{
        {
            {"true", true},
            {"True", true},
            {"1", true},
            {"y", true},
            {"yes", true},
            {"oui", true},
            {"v", true},
            {"vrai", true},
            {"false", false},
            {"False", false},
            {"0", false},
            {"n", false},
            {"no", false},
            {"non", false},
            {"f", false},
            {"faux", false},
        }
    };
    const Explorer::Column col = m_col;
    QModelIndex index = sourceModel()->index(srcRow, (int)col, srcParent);
    switch(col)
    {
        case Explorer::Column::Name:
        case Explorer::Column::Value:
        case Explorer::Column::Min:
        case Explorer::Column::Max:
            return sourceModel()->data(index).toString().contains(filterRegExp());

        case Explorer::Column::Get:
        case Explorer::Column::Set:
        {
            auto data = sourceModel()->data(index, Qt::CheckStateRole).toBool();
            auto it = values.find(filterRegExp().pattern());
            if(it != values.end())
            {
                return it.value() == data;
            }
            return false;
        }
        default:
            return false;
    }
}

bool
DeviceExplorerFilterProxyModel::hasAcceptedChildren(int srcRow, const QModelIndex& srcParent) const
{
    QModelIndex index = sourceModel()->index(srcRow, 0, srcParent);

    if(! index.isValid())
    {
        return false;
    }

    ISCORE_ASSERT(index.model());
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
}
