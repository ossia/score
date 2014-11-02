/*
Copyright: LaBRI / SCRIME

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

#include "mainwindow.hpp"
#include "ui_mainwindow.h"
#include "../timeEvent/timeevent.hpp"
#include "../timeEvent/timeeventview.hpp"
#include "../timeBox/timeboxmodel.hpp"
#include "../timeBox/timeboxpresenter.hpp"
#include "../timeBox/timeboxsmallview.hpp"
#include "utils.hpp"
#include "graphicsview.hpp"
#include "../timeBox/timeboxfullview.hpp"

#include <QMouseEvent>
#include <QAction>
#include <QActionGroup>
#include <QStateMachine>
#include <QPointF>
#include <QTimer>
#include <QFinalState>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
  : QMainWindow(parent), ui(new Ui::MainWindow)
{
  ui->setupUi(this); /// Bounding with mainwindow.ui
  setWindowTitle(tr("%1").arg(QApplication::applicationName()));

  createGraphics();
  createActions();
  createActionGroups();
  createStates();
  createTransitions();
  createConnections();

  QTimer::singleShot(0, _stateMachine, SLOT(start())); /// Using a single shot timer to ensure that the window is fully constructed before we start processing
}

void MainWindow::createGraphics()
{
  _pView = ui->graphicsView;

  _pMainTimebox = new Timebox(0, _pView, QPointF(0,0), 1000, 500, FULL); ///@todo adapter dynamiquement la taille du scénario
  Q_CHECK_PTR(_pMainTimebox);
  _pMainTimebox->setParent(this);
  connect(_pMainTimebox, SIGNAL(isFull()), this, SLOT(changeCurrentTimeboxScene()));
  setcurrentTimebox(_pMainTimebox);

  _timeBar = new TimeBarWidget(_pMainTimebox->getView());
}

void MainWindow::createActions() {
  _pDeleteAction = new QAction(tr("Delete"), this);
  /// QKeySequence(Qt::CTRL + Qt::Key_Backspace) = Command + backSpace in MacOSX (see QKeySequence doc)
  _pDeleteAction->setShortcuts(QList<QKeySequence>() << QKeySequence(Qt::CTRL + Qt::Key_Backspace) << QKeySequence::Delete); /// @bug Bug dans Qt pour les raccourcis mac https://bugreports.qt-project.org/browse/QTBUG-33015 (by jC)
  //ui->menuEdit->addAction(_pDeleteAction);
}

void MainWindow::createActionGroups()
{
  /// GraphicsItems type stocked in relative's actions, used in MainWindow::addItem()
  ui->actionAddTimeEvent->setData(EventItemType);
  ui->actionAddTimeBox->setData(BoxItemType);

  /// @todo Faire un stateMachine dédié pour gestion de la actionBar à la omnigraffle (par jC)
  _pMouseActionGroup = new QActionGroup(this);
  _pMouseActionGroup->addAction(ui->actionAddTimeEvent);
  _pMouseActionGroup->addAction(ui->actionAddTimeBox);
  /// @bug QActionGroup always return 0 in checkedAction() if we set m_mouseActionGroup->setExclusive(false); (by jC)

  /// Mouse cursor relative's actions
  ui->actionMouse->setData(QGraphicsView::NoDrag);
  ui->actionScroll->setData(QGraphicsView::ScrollHandDrag);
  ui->actionSelect->setData(QGraphicsView::RubberBandDrag);

  _pMouseActionGroup->addAction(ui->actionMouse);
  _pMouseActionGroup->addAction(ui->actionScroll);
  _pMouseActionGroup->addAction(ui->actionSelect);

  ui->actionMouse->setChecked(true);
}

void MainWindow::createStates()
{
  _stateMachine = new QStateMachine(this);

  _initialState = new QState();
  _initialState->assignProperty(this, "objectName", tr("mainWindow"));
  //_initialState->assignProperty(this, "currentTimebox", qVariantFromValue((Timebox)_MainTimebox)); Si on veut un jour stocker la currentTimebox en tant que Q_PROPERTY, pour gérer la navigation hiérarchique depuis le StateMachine. Abandonné car surement trop compliqué pour pas grand chose. sinon http://blog.bigpixel.ro/2010/04/storing-pointer-in-qvariant/
  _initialState->assignProperty(_pMouseActionGroup, "enabled", true);
  _stateMachine->addState(_initialState);
  _stateMachine->setInitialState(_initialState);

  /// creating a new top-level state
  _normalState = new QState();
  _editionState = new QState(_normalState);
  _editionState->assignProperty(_pMouseActionGroup, "enabled", true);

  /// @todo create a state when changing the _currenFullView. do it history state or parallel (because can occur during execution or editing) (by jC)
  _executionState = new QState(_normalState);
  _executionState->assignProperty(_pMouseActionGroup, "enabled", false);

  _runningState = new QState(_executionState);
  _pausedState = new QState(_executionState);
  _stoppedState = new QState(_executionState);
  _executionState->setInitialState(_runningState);

  _normalState->setInitialState(_editionState);
  _stateMachine->addState(_normalState);

  _finalState = new QFinalState(); /// @todo gérer le final state et la suppression d'objets graphiques
  _stateMachine->addState(_finalState);
}

void MainWindow::createTransitions()
{
  _initialState->addTransition(_initialState, SIGNAL(propertiesAssigned()), _normalState);
/*
  _editionState->addTransition(ui->playButton, SIGNAL(clicked()), _runningState);
  _runningState->addTransition(ui->playButton, SIGNAL(clicked()), _pausedState);
  _runningState->addTransition(ui->stopButton, SIGNAL(clicked()), _stoppedState);
  _pausedState->addTransition(ui->playButton, SIGNAL(clicked()), _runningState);
  _pausedState->addTransition(ui->stopButton, SIGNAL(clicked()), _stoppedState);
  */
  _stoppedState->addTransition(_stoppedState, SIGNAL(propertiesAssigned()), _editionState);

  _normalState->addTransition(this, SIGNAL(destroyed()), _finalState);
}

void MainWindow::createConnections()
{
//  connect(ui->graphicsView, SIGNAL(mousePressAddItem(QPointF)), this, SLOT(addItem(QPointF)));
  connect(_pMouseActionGroup, SIGNAL(triggered(QAction*)), ui->graphicsView, SLOT(mouseDragMode(QAction*)));
  connect(ui->graphicsView, SIGNAL(mousePosition(QPointF)), this, SLOT(setMousePosition(QPointF)));
  connect(ui->headerWidget, SIGNAL(doubleClicked()), this, SLOT(headerWidgetClicked()));
  connect(_pDeleteAction, SIGNAL(triggered()), this, SLOT(deleteSelectedItems()));
}
/*
void MainWindow::addItem(QPointF pos)
{
  QAction *action = _pMouseActionGroup->checkedAction();
  Q_ASSERT(action);
  if(action != ui->actionAddTimeEvent && action != ui->actionAddTimeBox) { /// @todo  pas très découplé mais à cause de la galère des groupActions, regarder signalMapper
      return;
    }

  qint32 type = action->data().toInt(); /// we recover the data associated with the action (see createActionGroups())

  Q_ASSERT(type);
  if (type == EventItemType) {
      new TimeEvent(_pCurrentTimebox, pos);
    }
  else if(type == BoxItemType) {
      new Timebox(_pCurrentTimebox, _pView, pos, 300, 200, SMALL);
    }

  ui->actionMouse->setChecked(true); /// @todo Pas joli, à faire dans la méthode dirty ou  dans un stateMachine (jc)
}
*/
void MainWindow::headerWidgetClicked()
{
  _pCurrentTimebox->goSmall();
}

void MainWindow::deleteSelectedItems()
{
  QList<QGraphicsItem*> items = _pCurrentTimebox->fullView()->selectedItems();
  if (items.isEmpty()) {
      return;
    }

  QListIterator<QGraphicsItem*> list(items);
  while (list.hasNext()) {
      QGraphicsItem *item = list.next();
      if(TimeboxSmallView *tb = qgraphicsitem_cast<TimeboxSmallView*>(item)){
          tb->deleteLater();
        }
      else if(TimeEventView *te = qgraphicsitem_cast<TimeEventView*>(item)){
          te->deleteLater();
        }
    }
}

void MainWindow::changeCurrentTimeboxScene()
{
  setcurrentTimebox(qobject_cast<Timebox*>(sender()));
  ui->headerWidget->changeName(_pCurrentTimebox->model()->name()); /// Actualize headerWidget's name according to the current Timebox
}

void MainWindow::setMousePosition(QPointF point)
{
  statusBar()->showMessage(QString("position : %1 %2").arg(point.x()).arg(point.y()));
}

void MainWindow::setcurrentTimebox(Timebox *arg)
{
  if (_pCurrentTimebox != arg) {
      _pCurrentTimebox = arg;
    }
}
