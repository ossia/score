#pragma once

#include <QTreeView>

class DeviceExplorerModel;
class DeviceExplorerFilterProxyModel;

class DeviceExplorerView : public QTreeView
{
        Q_OBJECT

    public:

        DeviceExplorerView(QWidget* parent = 0);
        ~DeviceExplorerView();

        void setModel(QAbstractItemModel* model) override;
        void setModel(DeviceExplorerFilterProxyModel* model);

        DeviceExplorerModel* model();
        const DeviceExplorerModel* model() const;

        int getIOTypeColumn() const;

        bool hasCut() const;
        void copy();
        void cut();
        void paste();

        void moveUp();
        void moveDown();
        void promote();
        void demote();

        QModelIndexList selectedIndexes() const override;
        QModelIndex selectedIndex() const;

    signals:
        void selectionChanged();

    protected slots:
        virtual void selectionChanged(const QItemSelection& selected, const QItemSelection& deselected) override;

        void headerMenuRequested(const QPoint& pos);
        void columnVisibilityChanged(bool shown);

    protected:

        //virtual void closeEvent(QCloseEvent *event) override;

        void setSelectedIndex(const QModelIndex& index);

        void installStyleSheet();

        void saveSettings();
        void restoreSettings();
        void setInitialColumnsSizes();

        void initActions();

    protected:
        QList<QAction*> m_actions;

        bool m_hasProxy;

};

