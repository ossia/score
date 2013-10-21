/*
Copyright: LaBRI / SCRIME

Author: Jaime Chao (01/10/2013)

This software is governed by the CeCILL license under French law and
abiding by the rules of distribution of free software.  You can  use,
modify and/ or redistribute the software under the terms of the CeCILL
license as circulated by CEA, CNRS and INRIA at the following URL
"http://www.cecill.info".

As a counterpart to the access to the source code and  rights to copy,
modify and redistribute granted by the license, users are provided only
with a limited warranty  and the software's author,  the holder of the
economic rights,  and the successive licensors  have only  limited
liability.

In this respect, the user's attention is drawn to the risks associated
with loading,  using,  modifying and/or developing or reproducing the
software by the user in light of its specific status of free software,
that may mean  that it is complicated to manipulate,  and  that  also
therefore means  that it is reserved for developers  and  experienced
professionals having in-depth computer knowledge. Users are therefore
encouraged to load and test the software's suitability as regards their
requirements in conditions enabling the security of their systems and/or
data to be ensured and,  more generally, to use and operate it in the
same conditions as regards security.

The fact that you are presently reading this means that you have had
knowledge of the CeCILL license and that you accept its terms.
*/

#include "itemTypes.hpp"
#include "mainwindow.hpp"
#include "ui_mainwindow.h"
#include "graphicstimeevent.hpp"
#include "graphicstimeprocess.hpp"
#include <QMouseEvent>
#include <QActionGroup>
#include <QGraphicsView>
#include <QPointF>

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

  //QTimer::singleShot(0, &stateMachine, SLOT(start())); /// Using a single shot timer to ensure that the window is fully constructed before we start processing

}

MainWindow::~MainWindow()
{
  delete ui;
}

void MainWindow::createActionGroups() /// @todo Faire un stateMachine dédié pour gestion à la omnigraffle
{
  // GraphicsItems relative's actions
  ui->actionAddTimeEvent->setData(EventItemType);
  ui->actionAddTimeProcess->setData(ProcessItemType);

  m_mouseActionGroup = new QActionGroup(this); // actiongroup keeping all mouse relatives actions
  m_mouseActionGroup->addAction(ui->actionAddTimeEvent);
  m_mouseActionGroup->addAction(ui->actionAddTimeProcess);
  /// @bug QActionGroup always return 0 in checkedAction() if we set m_mouseActionGroup->setExclusive(false);

  // Mouse cursor relative's actions
  ui->actionMouse->setData(QGraphicsView::NoDrag);
  ui->actionScroll->setData(QGraphicsView::ScrollHandDrag);
  ui->actionSelect->setData(QGraphicsView::RubberBandDrag);

  m_mouseActionGroup->addAction(ui->actionMouse);
  m_mouseActionGroup->addAction(ui->actionScroll);
  m_mouseActionGroup->addAction(ui->actionSelect);

  ui->actionMouse->setChecked(true);
}

void MainWindow::createConnections()
{
  connect(ui->graphicsView, SIGNAL(mousePressAddItem(QPointF)), this, SLOT(addItem(QPointF)));

  connect(m_mouseActionGroup, SIGNAL(triggered(QAction*)), ui->graphicsView, SLOT(mouseDragMode(QAction*)));
  connect(ui->graphicsView, SIGNAL(mousePosition(QPointF)), this, SLOT(setMousePosition(QPointF)));
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

void MainWindow::addItem(QPointF pos)
{
  QAction *action = m_mouseActionGroup->checkedAction();
  Q_ASSERT(action);
  if(action != ui->actionAddTimeEvent && action != ui->actionAddTimeProcess) { /// @todo  pas très découplé mais à cause de la galère des groupActions, regarder signalMapper
      return;
    }

  qint32 type = action->data().toInt(); // we recover the data associated with the action (see createActionGroups())
  QObject *item = NULL;

  Q_ASSERT(type);
  if (type == EventItemType) {
      item = new GraphicsTimeEvent(pos, 0, _scene);
    }
  else if(type == ProcessItemType) {
      item = new GraphicsTimeProcess(pos, 0, _scene);
    }
  ui->actionMouse->setChecked(true); /// @todo Pas joli, à faire dans la méthode dirty ou  dans un stateMachine

  Q_CHECK_PTR(item);
  if(item) {
      connectItem(item);
      setDirty(true);
    }
}

void MainWindow::connectItem(QObject *item)
{
    connect(item, SIGNAL(dirty()), this, SLOT(setDirty()));

    const QMetaObject *metaObject = item->metaObject();
    if (metaObject->indexOfSignal("playOrPauseButtonClicked()") > -1)
      connect(ui->playButton, SIGNAL(clicked()), item, SIGNAL(playOrPauseButtonClicked())); /// @todo rename playButton to playOrPauseButton and transform it
    if (metaObject->indexOfSignal("stopButtonClicked()") > -1)
      connect(ui->stopButton, SIGNAL(clicked()), item, SIGNAL(stopButtonClicked()));
    if(metaObject->indexOfSignal("headerClicked()") > -1)
      connect(item, SIGNAL(headerClicked()), ui->graphicsView, SLOT(graphicItemEnsureVisible()));
//    if (metaObject->indexOfProperty("running") > -1) /// @todo change play button to play/pause
//      connect(ui->playButton, SIGNAL(clicked()), item, SLOT(setrunning(true;)));
//    if (metaObject->indexOfProperty("stopped"))
//      connect(ui->stopButton, SIGNAL(clicked()), item, SLOT(setstopped(false;)));
}

void MainWindow::setMousePosition(QPointF point)
{
  statusBar()->showMessage(QString("position : %1 %2").arg(point.x()).arg(point.y()));
}

void MainWindow::mousePressEvent(QMouseEvent *event)
{
  QMainWindow::mousePressEvent(event);
}


void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
  QMainWindow::mouseMoveEvent(event);

}
