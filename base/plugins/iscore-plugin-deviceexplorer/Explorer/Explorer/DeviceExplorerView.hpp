#pragma once

#include <QTreeView>

class DeviceExplorerModel;
class DeviceExplorerFilterProxyModel;

class DeviceExplorerView : public QTreeView
{
        Q_OBJECT

    public:
        explicit DeviceExplorerView(QWidget* parent = 0);
        ~DeviceExplorerView();

        void setModel(QAbstractItemModel* model) override;
        void setModel(DeviceExplorerFilterProxyModel* model);

        DeviceExplorerModel* model();
        const DeviceExplorerModel* model() const;

        void setSelectedIndex(const QModelIndex& index);

        QModelIndexList selectedIndexes() const override;
        QModelIndex selectedIndex() const;

        bool hasProxy() const;

    signals:
        void selectionChanged();

    protected slots:
        virtual void selectionChanged(const QItemSelection& selected, const QItemSelection& deselected) override;

        void headerMenuRequested(const QPoint& pos);
        void columnVisibilityChanged(bool shown);

    protected:
        void saveSettings();
        void restoreSettings();
        void setInitialColumnsSizes();

        void initActions();

    protected:
        QList<QAction*> m_actions;

        bool m_hasProxy;

};

