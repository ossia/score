#include "DeviceExplorerFilterProxyModel.hpp"
#include "DeviceExplorerModel.hpp"
#include "DeviceExplorerView.hpp"

class QItemSelection;
class QPoint;
class QWidget;

#ifdef MODEL_TEST
#include "ModelTest/modeltest.h"
#endif

#include <qabstractitemview.h>
#include <qabstractproxymodel.h>
#include <qaction.h>
#include <qbytearray.h>
#include <qfile.h>
#include <qheaderview.h>
#include <qiodevice.h>
#include <qmenu.h>
#include <qnamespace.h>
#include <qobject.h>
#include <qsettings.h>
#include <qstring.h>
#include <qtypetraits.h>
#include <qvariant.h>
#include <iostream> //DEBUG

namespace
{
    const QString GeometrySetting("DeviceExplorerView/Geometry");
    const QString HeaderViewSetting("DeviceExplorerView/HeaderView");
}

DeviceExplorerView::DeviceExplorerView(QWidget* parent)
    : QTreeView(parent),
      m_hasProxy(false)
{

    setAllColumnsShowFocus(true);

    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    //cf http://qt-project.org/doc/qt-5/qabstractitemview.html#SelectionBehavior-enum


    header()->setContextMenuPolicy(Qt::CustomContextMenu);  //header will emit the signal customContextMenuRequested()
    connect(header(), SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(headerMenuRequested(const QPoint&)));


    //- Drag'n Drop.

    setDragEnabled(true);
    setAcceptDrops(true);
    setUniformRowHeights(true);

    //setDragDropMode(QAbstractItemView::InternalMove); //The view accepts move (not copy) operations only from itself.
    setDragDropMode(QAbstractItemView::DragDrop);

    setDropIndicatorShown(true);

    setDefaultDropAction(Qt::MoveAction);  //default DefaultDropAction is CopyAction

    setDragDropOverwriteMode(false);

    /*
      - if setDragDropMode(QAbstractItemView::InternalMove); : it is impossible to have copyAction in model::dropMimeData().
           -- if model::supportedDragActions & supportedDropActions() return only Copy, it is visually forbidden to drop (model::dropMimeData() is never called)
           -- if model::supportedDragActions & supportedDropActions() return Move|Copy, Action in model::dropMimeData() will always be a Move.

      - if setDragDropMode(QAbstractItemView::DragDrop); : we only have copyAction in model::dropMimeData(). [even if model::supportedDragActions & supportedDropActions() return Move|Copy ; even if setDragDropOverwriteMode(false or true) ]
           ==> setDefaultDropAction(Qt::MoveAction); is needed to have CopyAction

     */

    QFile f(":/DeviceExplorer.qss");
    if(f.open(QFile::ReadOnly))
        setStyleSheet(f.readAll());
}


DeviceExplorerView::~DeviceExplorerView() //void DeviceExplorerView::closeEvent(QCloseEvent *event)
{
    saveSettings();
}

void
DeviceExplorerView::setInitialColumnsSizes()
{
    ISCORE_ASSERT(model());

    header()->resizeSection((int)DeviceExplorerModel::Column::Name, 220);
    header()->resizeSection((int)DeviceExplorerModel::Column::Value, 50);
    header()->resizeSection((int)DeviceExplorerModel::Column::Get, 36);
    header()->resizeSection((int)DeviceExplorerModel::Column::Set, 36);
    header()->resizeSection((int)DeviceExplorerModel::Column::Min, 50);
    header()->resizeSection((int)DeviceExplorerModel::Column::Max, 50);
}


void
DeviceExplorerView::saveSettings()
{
    QSettings settings;
    settings.setValue(GeometrySetting, saveGeometry());
    settings.setValue(HeaderViewSetting, header()->saveState());
}

void
DeviceExplorerView::restoreSettings()
{
    QSettings settings;
    restoreGeometry(settings.value(GeometrySetting).toByteArray());
    header()->restoreState(settings.value(HeaderViewSetting).toByteArray());
}

void
DeviceExplorerView::setModel(QAbstractItemModel* model)
{
    QTreeView::setModel(model);
    m_hasProxy = false;
    #ifdef MODEL_TEST
    qDebug() << "*** ADD MODEL_TEST\n";
    (void) new ModelTest(model, this);
    #endif

    if(model)
    {
        setInitialColumnsSizes();
        restoreSettings();
        initActions(); //after restoreSettings() to have actions correctly initialized
    }
}

void
DeviceExplorerView::setModel(DeviceExplorerFilterProxyModel* model)
{
    QTreeView::setModel(static_cast<QAbstractItemModel*>(model));
    m_hasProxy = true;
    #ifdef MODEL_TEST
    qDebug() << "*** ADD MODEL_TEST\n";
    (void) new ModelTest(model->sourceModel(), this);
    #endif

    if(model)
    {
        setInitialColumnsSizes();
        restoreSettings();
        initActions(); //after restoreSettings() to have actions correctly initialized
    }
}

DeviceExplorerModel*
DeviceExplorerView::model()
{
    if(! m_hasProxy)
    {
        return static_cast<DeviceExplorerModel*>(QTreeView::model());
    }
    else
    {
        return static_cast<DeviceExplorerModel*>(static_cast<QAbstractProxyModel*>(QTreeView::model())->sourceModel());
    }
}

const DeviceExplorerModel*
DeviceExplorerView::model() const
{
    if(! m_hasProxy)
    {
        return static_cast<const DeviceExplorerModel*>(QTreeView::model());
    }
    else
    {
        return static_cast<const DeviceExplorerModel*>(static_cast<const QAbstractProxyModel*>(QTreeView::model())->sourceModel());
    }
}


void
DeviceExplorerView::initActions()
{
    ISCORE_ASSERT(model());
    const int n = model()->columnCount();
    m_actions.reserve(n);

    for(int i = 0; i < n; ++i)
    {
        QAction* a = new QAction(model()->headerData(i, Qt::Horizontal, Qt::DisplayRole).toString(), this);
        a->setCheckable(true);

        a->setChecked(!isColumnHidden(i));

        connect(a, SIGNAL(toggled(bool)), this, SLOT(columnVisibilityChanged(bool)));
        m_actions.append(a);
    }

}
bool DeviceExplorerView::hasProxy() const
{
    return m_hasProxy;
}


void
DeviceExplorerView::columnVisibilityChanged(bool shown)
{
    QAction* a = qobject_cast<QAction*> (sender());
    ISCORE_ASSERT(a);
    const int ind = m_actions.indexOf(a);
    ISCORE_ASSERT(ind != -1);
    setColumnHidden(ind, !shown);
}

void
DeviceExplorerView::headerMenuRequested(const QPoint& pos)
{
    QMenu contextMenu(this);
    const int n = m_actions.size();

    for(int i = 0; i < n; ++i)
    {
        contextMenu.addAction(m_actions.at(i));
    }

    contextMenu.exec(pos);
}

void
DeviceExplorerView::selectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
{
    QTreeView::selectionChanged(selected, deselected);

    /*
    {//DEBUG

      QModelIndexList l = selectedIndexes();
      QModelIndexList l0;
      foreach (const QModelIndex &index, l) {
      if (index.column() == (int)DeviceExplorerModel::Column::Name) {

        if (!index.isValid())
          std::cerr<<" !!! invalid index in selection !!!\n";

        if (! m_hasProxy)
    l0.append(index);
        else
    l0.append(static_cast<const QAbstractProxyModel *>(QTreeView::model())->mapToSource(index));
      }
    }
      model()->debug_printIndexes(l0);

    }
    */

    emit selectionChanged();
}

QModelIndexList
DeviceExplorerView::selectedIndexes() const
{
    //As we have done setSelectionBehavior(QAbstractItemView::SelectRows)
    // we get the indexes for each column corresponding to a selected row.
    //We want only the indexes of the NAME column.


    const int col = (int)DeviceExplorerModel::Column::Name;
    QModelIndexList l = QTreeView::selectedIndexes();

    QModelIndexList l0;
    foreach(const QModelIndex & index, l)
    {
        if(index.column() == col)
        {

            if(!index.isValid())
            {
                std::cerr << " !!! invalid index in selection !!!\n";
            }

            //if (! m_hasProxy)
                l0.append(index);
            //else
            //    l0.append(static_cast<const QAbstractProxyModel *>(QTreeView::model())->mapToSource(index));

            //REM: we append index in ProxyModel if m_hasProxy
            // it is necesary because drag'n drop will automatically call selectedIndexes() & mapToSource().
        }
    }

    return l0;
}


//REM: use selectedIndex() & setSelectedIndex()
// instead of currentIndex() & setCurrentIndex()
// to take into account proxy presence.

QModelIndex
DeviceExplorerView::selectedIndex() const
{
    if(! m_hasProxy)
    {
        return currentIndex();
    }
    else
    {
        return static_cast<const QAbstractProxyModel*>(QTreeView::model())->mapToSource(currentIndex());
    }
}

void
DeviceExplorerView::setSelectedIndex(const QModelIndex& index)
{
    if(! m_hasProxy)
    {
        return setCurrentIndex(index);
    }
    else
    {
        return setCurrentIndex(static_cast<const QAbstractProxyModel*>(QTreeView::model())->mapFromSource(index));
    }
}
