#pragma once

#include <QSortFilterProxyModel>
#include <Explorer/Explorer/Column.hpp>

class QModelIndex;
class QObject;


namespace Explorer
{
class DeviceExplorerFilterProxyModel final : public QSortFilterProxyModel
{
    public:
        explicit DeviceExplorerFilterProxyModel(QObject* parent = nullptr);

        void setColumn(Explorer::Column col);

    protected:
        bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

        bool filterAcceptsRowItself(int sourceRow, const QModelIndex& sourceParent) const;
        bool hasAcceptedChildren(int sourceRow, const QModelIndex& sourceParent) const;

    protected:
        Explorer::Column m_col;

};
}
