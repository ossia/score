#pragma once

#include <QAbstractItemModel>
#include <QList>
#include <QTreeView>
class QAction;
class QItemSelection;
class QPoint;
class QWidget;

namespace Explorer
{
class DeviceExplorerFilterProxyModel;
class DeviceExplorerModel;
class DeviceExplorerView final : public QTreeView
{
  Q_OBJECT

public:
  explicit DeviceExplorerView(QWidget* parent = nullptr);
  ~DeviceExplorerView();

  void setModel(QAbstractItemModel* model) override;
  void setModel(DeviceExplorerFilterProxyModel* model);

  DeviceExplorerModel* model();
  const DeviceExplorerModel* model() const;

  void setSelectedIndex(const QModelIndex& index);

  QModelIndexList selectedIndexes() const override;
  QModelIndex selectedIndex() const;

  bool hasProxy() const;

Q_SIGNALS:
  void selectionChanged();
  void created(QModelIndex parent, int start, int end);

private Q_SLOTS:
  void selectionChanged(
      const QItemSelection& selected,
      const QItemSelection& deselected) override;

  void headerMenuRequested(const QPoint& pos);
  void columnVisibilityChanged(bool shown);

private:
  void rowsInserted(const QModelIndex &parent, int start, int end) override;
  void saveSettings();
  void restoreSettings();
  void setInitialColumnsSizes();

  void initActions();

  QList<QAction*> m_actions;

  bool m_hasProxy;
};
}
