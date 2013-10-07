#include "mainwindow.hpp"
#include "ui_mainwindow.h"

const qint16 OFFSET_INCREMENT = 5;

MainWindow::MainWindow(QWidget *parent)
  : QMainWindow(parent), ui(new Ui::MainWindow)
{
  ui->setupUi(this);

  _scene = new QGraphicsScene(this);
  ui->graphicsView->setScene(_scene);
}

MainWindow::~MainWindow()
{
  delete ui;
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

  return ui->graphicsView->mapToScene(point - ui->graphicsView->pos()).toPoint();
}


void MainWindow::mousePressEvent(QMouseEvent *event)
{
  QMainWindow::mousePressEvent(event);
  //_scene->addLine(0,0,position().x(), position().y());
  QGraphicsItem *item = new GraphicsTimeEvent(position(), 0, _scene);
  statusBar()->showMessage(QString("position : %1 %2").arg(position().x()).arg(position().y()));
}


void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
  QMainWindow::mouseMoveEvent(event);

}
