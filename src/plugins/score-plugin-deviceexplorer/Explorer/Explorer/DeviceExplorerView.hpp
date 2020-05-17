#pragma once

#include <QAbstractItemModel>
#include <QList>
#include <QTreeView>

#include <verdigris>
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
  W_OBJECT(DeviceExplorerView)

public:
  explicit DeviceExplorerView(QWidget* parent = nullptr);
  ~DeviceExplorerView() override;

  void setModel(QAbstractItemModel* model) override;
  void setModel(DeviceExplorerFilterProxyModel* model);

  DeviceExplorerModel* model();
  const DeviceExplorerModel* model() const;

  void setSelectedIndex(const QModelIndex& index);

  QModelIndexList selectedIndexes() const override;
  QModelIndex selectedIndex() const;

  bool hasProxy() const;

public:
  void selectionChanged() W_SIGNAL(selectionChanged, ());
  void created(QModelIndex parent, int start, int end) W_SIGNAL(created, parent, start, end);

private:
  void selectionChanged(const QItemSelection& selected, const QItemSelection& deselected) override;
  W_SLOT(selectionChanged, (const QItemSelection&, const QItemSelection&));

  void headerMenuRequested(const QPoint& pos);
  W_SLOT(headerMenuRequested);
  void columnVisibilityChanged(bool shown);
  W_SLOT(columnVisibilityChanged);

private:
  void keyPressEvent(QKeyEvent*) override;

  QModelIndexList selectedDraggableIndexes() const;
  void startDrag(Qt::DropActions supportedActions) override;
  void rowsInserted(const QModelIndex& parent, int start, int end) override;
  void saveSettings();
  void restoreSettings();
  void setInitialColumnsSizes();

  void initActions();

  QList<QAction*> m_actions;

  bool m_hasProxy{};

  // QWidget interface
protected:
  void paintEvent(QPaintEvent* event) override;
};
}
