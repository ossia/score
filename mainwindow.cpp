#include "itemTypes.hpp"
#include "mainwindow.hpp"
#include "ui_mainwindow.h"
#include "graphicstimeevent.hpp"
#include "graphicstimeprocess.hpp"
#include <QMouseEvent>
#include <QActionGroup>
#include <QGraphicsView>

const qint16 OFFSET_INCREMENT = 5;

MainWindow::MainWindow(QWidget *parent)
  : QMainWindow(parent), ui(new Ui::MainWindow)
{
  ui->setupUi(this);
  setWindowTitle(tr("%1").arg(QApplication::applicationName()));

  _scene = new QGraphicsScene(this);
  ui->graphicsView->setScene(_scene);

  createActionGroups();
  createConnections();
}

MainWindow::~MainWindow()
{
  delete ui;
}

void MainWindow::createActionGroups()
{
  // GraphicsItems relative's actions
  ui->actionAddTimeEvent->setData(EventItemType);
  ui->actionAddTimeProcess->setData(ProcessItemType);

  m_addGraphicsItemActionGroup = new QActionGroup(this);
  m_addGraphicsItemActionGroup->addAction(ui->actionAddTimeEvent);
  m_addGraphicsItemActionGroup->addAction(ui->actionAddTimeProcess);

  // Mouse cursor relative's actions
  ui->actionMouse->setData(QGraphicsView::NoDrag);
  ui->actionScroll->setData(QGraphicsView::ScrollHandDrag);
  ui->actionSelect->setData(QGraphicsView::RubberBandDrag);

  _viewDragModeActionGroup = new QActionGroup(this);
  _viewDragModeActionGroup->addAction(ui->actionMouse);
  _viewDragModeActionGroup->addAction(ui->actionScroll);
  _viewDragModeActionGroup->addAction(ui->actionSelect);

  ui->actionMouse->setChecked(true);
}

void MainWindow::createConnections()
{
  connect(ui->actionAddTimeEvent, SIGNAL(triggered()), this, SLOT(addItem()));
  connect(ui->actionAddTimeProcess, SIGNAL(triggered()), this, SLOT(addItem()));

  connect(_viewDragModeActionGroup, SIGNAL(triggered(QAction*)), ui->graphicsView, SLOT(mouseDragMode(QAction*)));
  connect(ui->graphicsView, SIGNAL(mousePosition(QPoint)), this, SLOT(setMousePosition(QPoint)));
}

void MainWindow::setDirty(bool on)
{
    setWindowModified(on);
    updateUi();
}

void MainWindow::updateUi()
{
 /// @todo Update actions to reflect application state
 // ui->actionSelect->setChecked();
}

QPoint MainWindow::position() //p.426
{
  QPoint point = mapFromGlobal(QCursor::pos());
  if(!ui->graphicsView->geometry().contains(point)){
      point = _previousPoint.isNull() ? ui->graphicsView->pos() + QPoint(10,10) : _previousPoint;
    }
  if(!point.isNull() && point == _previousPoint){
      point += QPoint(_addOffset, _addOffset);
      _addOffset += OFFSET_INCREMENT;
    }
  else {
      _addOffset = OFFSET_INCREMENT;
      _previousPoint = point;
    }
  return ui->graphicsView->mapToScene(point).toPoint();
}

void MainWindow::addItem()
{
  QAction *action = qobject_cast<QAction*>(sender()); /// voir p349 de AQP
  qint32 type = action->data().toInt();
  QObject *item = NULL;

  Q_ASSERT(type);
  if (type == EventItemType) {
      item = new GraphicsTimeEvent(position(), 0, _scene);
    }
  else if(type == ProcessItemType) {
      item = new GraphicsTimeProcess(position(), 0, _scene);
    }

  Q_CHECK_PTR(item);
  if(item) {
      connectItem(item);
      setDirty(true);
    }
}

void MainWindow::connectItem(QObject *item)
{
    connect(item, SIGNAL(dirty()), this, SLOT(setDirty()));
    /// @todo Add metaobject connect
    //const QMetaObject *metaObject = item->metaObject();
    //if (metaObject->indexOfProperty("brush") > -1)
        //connect(brushWidget, SIGNAL(brushChanged(const QBrush&)), item, SLOT(setBrush(const QBrush&)));
}

void MainWindow::setMousePosition(QPoint point)
{
  statusBar()->showMessage(QString("position : %1 %2").arg(point.x()).arg(point.y()));
}

void MainWindow::mousePressEvent(QMouseEvent *event)
{
  QMainWindow::mousePressEvent(event);
  //_scene->addLine(0,0,position().x(), position().y());
}


void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
  QMainWindow::mouseMoveEvent(event);

}
