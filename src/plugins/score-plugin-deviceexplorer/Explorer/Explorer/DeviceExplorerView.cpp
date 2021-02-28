// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "DeviceExplorerView.hpp"

#include "DeviceExplorerFilterProxyModel.hpp"
#include "DeviceExplorerModel.hpp"

#include <score/model/Skin.hpp>
#include <score/widgets/Pixmap.hpp>

#include <QAbstractItemView>
#include <QAbstractProxyModel>
#include <QAction>
#include <QDebug>
#include <QDrag>
#include <QFile>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMenu>
#include <QPainter>
#include <QSettings>
#include <QString>
#include <qnamespace.h>
#include <qtypetraits.h>

W_REGISTER_ARGTYPE(QItemSelection)

#include <wobjectimpl.h>
W_OBJECT_IMPL(Explorer::DeviceExplorerView)
namespace
{
const QString GeometrySetting("DeviceExplorerView/Geometry");
const QString HeaderViewSetting("DeviceExplorerView/HeaderView");
}

namespace Explorer
{
DeviceExplorerView::DeviceExplorerView(QWidget* parent) : QTreeView(parent), m_hasProxy(false)
{
  setAllColumnsShowFocus(true);

  setSelectionBehavior(QAbstractItemView::SelectRows);
  setSelectionMode(QAbstractItemView::ExtendedSelection);
  setAlternatingRowColors(true);

  header()->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(
      header(),
      &QWidget::customContextMenuRequested,
      this,
      &DeviceExplorerView::headerMenuRequested);

  header()->setMaximumHeight(20);
  //- Drag'n Drop.

  setDragEnabled(true);
  setAcceptDrops(true);
  setUniformRowHeights(true);

  setDragDropMode(QAbstractItemView::DragDrop);
  setDropIndicatorShown(true);

  setDefaultDropAction(Qt::MoveAction);

  setDragDropOverwriteMode(false);
}

DeviceExplorerView::~DeviceExplorerView()
{
  saveSettings();
}
QModelIndexList DeviceExplorerView::selectedDraggableIndexes() const
{
  QModelIndexList indexes = selectedIndexes();
  auto m = QTreeView::model();
  auto isNotDragEnabled
      = [m](const QModelIndex& index) { return !(m->flags(index) & Qt::ItemIsDragEnabled); };
  indexes.erase(std::remove_if(indexes.begin(), indexes.end(), isNotDragEnabled), indexes.end());
  return indexes;
}

void DeviceExplorerView::startDrag(Qt::DropActions)
{
  QModelIndexList indexes = selectedDraggableIndexes();
  if (indexes.count() > 0)
  {
    QMimeData* data = QTreeView::model()->mimeData(indexes);
    if (!data)
      return;

    QDrag* drag = new QDrag(this);
    drag->setMimeData(data);
    /*
        auto p =
       score::get_pixmap(QStringLiteral(":/icons/cursor_drag_device.png"));
        drag->setPixmap(QPixmap());
        drag->setDragCursor(p, Qt::CopyAction);
        drag->setDragCursor(p, Qt::MoveAction);
        drag->setDragCursor(p, Qt::LinkAction);
        drag->setDragCursor(p, Qt::ActionMask);
        drag->setDragCursor(p, Qt::IgnoreAction);
        drag->setDragCursor(p, Qt::TargetMoveAction);
        */
    drag->exec();
  }
}

void DeviceExplorerView::setInitialColumnsSizes()
{
  SCORE_ASSERT(model());

  header()->resizeSection((int)Explorer::Column::Name, 180);
  header()->resizeSection((int)Explorer::Column::Value, 50);
}

void DeviceExplorerView::saveSettings()
{
#if !defined(__EMSCRIPTEN__)
  QSettings settings;
  settings.setValue(GeometrySetting, saveGeometry());
  settings.setValue(HeaderViewSetting, header()->saveState());
#endif
}

void DeviceExplorerView::restoreSettings()
{
#if !defined(__EMSCRIPTEN__)
  QSettings settings;
  restoreGeometry(settings.value(GeometrySetting).toByteArray());
  header()->restoreState(settings.value(HeaderViewSetting).toByteArray());
  this->updateGeometry();
#endif
}

void DeviceExplorerView::setModel(QAbstractItemModel* model)
{
  m_hasProxy = false;
  QTreeView::setModel(model);
  // TODO review the save/restore system
  if (model)
  {
    setInitialColumnsSizes();
    //        restoreSettings();
    initActions(); // after restoreSettings() to have actions correctly
    // initialized
  }
}

void DeviceExplorerView::setModel(DeviceExplorerFilterProxyModel* model)
{
  QTreeView::setModel(static_cast<QAbstractItemModel*>(model));
  m_hasProxy = true;

  if (model)
  {
    setInitialColumnsSizes();
    //        restoreSettings();
    initActions(); // after restoreSettings() to have actions correctly
    // initialized
  }
}

DeviceExplorerModel* DeviceExplorerView::model()
{
  if (!m_hasProxy)
  {
    return static_cast<DeviceExplorerModel*>(QTreeView::model());
  }
  else
  {
    return static_cast<DeviceExplorerModel*>(
        static_cast<QAbstractProxyModel*>(QTreeView::model())->sourceModel());
  }
}

const DeviceExplorerModel* DeviceExplorerView::model() const
{
  auto m = QTreeView::model();
  if (!m)
    return nullptr;

  if (!m_hasProxy)
  {
    return static_cast<const DeviceExplorerModel*>(m);
  }
  else
  {
    auto model = static_cast<const QAbstractProxyModel*>(m);
    return qobject_cast<const DeviceExplorerModel*>(model->sourceModel());
  }
}

void DeviceExplorerView::initActions()
{
  SCORE_ASSERT(model());
  const int n = model()->columnCount();
  m_actions.reserve(n);

  for (int i = 0; i < n; ++i)
  {
    QAction* a
        = new QAction(model()->headerData(i, Qt::Horizontal, Qt::DisplayRole).toString(), this);
    a->setCheckable(true);

    a->setChecked(!isColumnHidden(i));

    connect(a, &QAction::toggled, this, &DeviceExplorerView::columnVisibilityChanged);
    m_actions.append(a);
  }
}
bool DeviceExplorerView::hasProxy() const
{
  return m_hasProxy;
}

void DeviceExplorerView::columnVisibilityChanged(bool shown)
{
  QAction* a = qobject_cast<QAction*>(sender());
  SCORE_ASSERT(a);
  const int ind = m_actions.indexOf(a);
  SCORE_ASSERT(ind != -1);
  setColumnHidden(ind, !shown);
}

void DeviceExplorerView::keyPressEvent(QKeyEvent* k)
{
  if (k->key() == Qt::Key_Escape)
  {
    clearSelection();
    k->accept();
  }
  else
  {
    QTreeView::keyPressEvent(k);
  }
}

void DeviceExplorerView::rowsInserted(const QModelIndex& parent, int start, int end)
{
  QTreeView::rowsInserted(parent, start, end);
  created(parent, start, end);
}

void DeviceExplorerView::headerMenuRequested(const QPoint& pos)
{
  QMenu contextMenu(this);
  const int n = m_actions.size();

  for (int i = 0; i < n; ++i)
  {
    contextMenu.addAction(m_actions.at(i));
  }

  contextMenu.exec(pos);
}

void DeviceExplorerView::selectionChanged(
    const QItemSelection& selected,
    const QItemSelection& deselected)
{
  QTreeView::selectionChanged(selected, deselected);
  selectionChanged();
}

QModelIndexList DeviceExplorerView::selectedIndexes() const
{
  // As we have done setSelectionBehavior(QAbstractItemView::SelectRows)
  // we get the indexes for each column corresponding to a selected row.
  // We want only the indexes of the NAME column.

  const int col = (int)Explorer::Column::Name;
  QModelIndexList l = QTreeView::selectedIndexes();

  QModelIndexList l0;
  Q_FOREACH (const QModelIndex& index, l)
  {
    if (index.column() == col)
    {

      if (!index.isValid())
      {
        qDebug() << " !!! invalid index in selection !!!";
      }

      l0.append(index);
      // REM: we append index in ProxyModel if m_hasProxy
      // it is necesary because drag'n drop will automatically call
      // selectedIndexes() & mapToSource().
    }
  }

  return l0;
}

// REM: use selectedIndex() & setSelectedIndex()
// instead of currentIndex() & setCurrentIndex()
// to take into account proxy presence.

QModelIndex DeviceExplorerView::selectedIndex() const
{
  if (!m_hasProxy)
  {
    return currentIndex();
  }
  else
  {
    return static_cast<const QAbstractProxyModel*>(QTreeView::model())
        ->mapToSource(currentIndex());
  }
}

void DeviceExplorerView::setSelectedIndex(const QModelIndex& index)
{
  if (!m_hasProxy)
  {
    return setCurrentIndex(index);
  }
  else
  {
    return setCurrentIndex(
        static_cast<const QAbstractProxyModel*>(QTreeView::model())->mapFromSource(index));
  }
}
void DeviceExplorerView::paintEvent(QPaintEvent* event)
{
  QTreeView::paintEvent(event);
  if (model() && model()->rowCount(rootIndex()) > 0)
    return;

  QPainter p{this->viewport()};
  const auto& skin = score::Skin::instance();
  auto font = skin.Bold12Pt;
  font.setPointSize(24);
  p.setFont(font);
  auto pen = p.pen();
  auto col = pen.color();
  col.setAlphaF(0.5);
  pen.setColor(col);
  p.setPen(pen);
  p.drawText(rect(), Qt::AlignCenter, "Right-click\nto add a device");
}
}
