#include "itemTypes.hpp"
#include "mainwindow.hpp"
#include "ui_mainwindow.h"

const qint16 OFFSET_INCREMENT = 5;

MainWindow::MainWindow(QWidget *parent)
  : QMainWindow(parent), ui(new Ui::MainWindow)
{
  ui->setupUi(this);

  _scene = new QGraphicsScene(this);
  ui->graphicsView->setScene(_scene);

  ui->actionAddTimeEvent->setData(EventItemType);
  ui->actionAddTimeProcess->setData(ProcessItemType);

  connect(ui->actionAddTimeEvent, SIGNAL(triggered()), this, SLOT(addItem()));
  connect(ui->actionAddTimeProcess, SIGNAL(triggered()), this, SLOT(addItem()));

  connect(ui->graphicsView, SIGNAL(mousePosition(QPoint)), this, SLOT(setMousePosition(QPoint)));
}

MainWindow::~MainWindow()
{
  delete ui;
}

void MainWindow::setDirty(bool on)
{
    setWindowModified(on);
    updateUi();
}

void MainWindow::setMousePosition(QPoint point)
{
  statusBar()->showMessage(QString("position : %1 %2").arg(point.x()).arg(point.y()));
}

void MainWindow::updateUi()
{
 /// @todo Update actions to reflect application state
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

  if (type == EventItemType)
    item = new GraphicsTimeEvent(position(), 0, _scene);
  else if(type == ProcessItemType)
    item = new GraphicsTimeProcess(position(), 0, _scene);
  else
    Q_ASSERT(false);

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


void MainWindow::mousePressEvent(QMouseEvent *event)
{
  QMainWindow::mousePressEvent(event);
  //_scene->addLine(0,0,position().x(), position().y());
}


void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
  QMainWindow::mouseMoveEvent(event);

}
