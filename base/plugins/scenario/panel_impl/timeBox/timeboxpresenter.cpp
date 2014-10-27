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

class TTTimeProcess;

#include "timeboxpresenter.hpp"
#include "automationview.hpp"
#include "timeboxmodel.hpp"
#include "timeboxsmallview.hpp"
#include "timeboxfullview.hpp"
#include "timeboxstorey.hpp"
#include "pluginview.hpp"
#include "scenarioview.hpp"
#include "graphicsview.hpp"
#include "timebox.hpp"
#include "statedebug.hpp"

#include <QDebug>
#include <QStateMachine>
#include <QFinalState>
#include <QState>
#include <iostream>

TimeboxPresenter::TimeboxPresenter(TimeboxModel *pModel, TimeboxSmallView *pSmallView, GraphicsView *pView, Timebox *parent)
  : QObject(parent), _mode(SMALL), _pTimebox(parent), _pModel(pModel), _pSmallView(pSmallView), _pFullView(NULL), _pGraphicsView(pView)
{  
  createStateMachine();
}

TimeboxPresenter::TimeboxPresenter(TimeboxModel *pModel, TimeboxFullView *pFullView, GraphicsView *pView, Timebox *parent)
  : QObject(parent), _mode(FULL), _pTimebox(parent), _pModel(pModel), _pSmallView(NULL), _pFullView(pFullView), _pGraphicsView(pView)
{
  createStateMachine();
}

TimeboxPresenter::~TimeboxPresenter()
{
  delete _pInitialState;
  delete _pNormalState; /// will delete all child states
  delete _pFinalState;
}

void TimeboxPresenter::createStateMachine()
{
  createStates();
  createTransitions();
  createConnections();
  _pStateMachine->start();
}

void TimeboxPresenter::createStates()
{
  _pStateMachine = new QStateMachine(this);

  _pInitialState = new QState();
  _pInitialState->assignProperty(this, "objectName", tr("BoxPresenter"));
  _pStateMachine->addState(_pInitialState);
  _pStateMachine->setInitialState(_pInitialState);

  // creating a new top-level state
  _pNormalState = new QState();

  _pSmallSizeState = new StateDebug("smallSize", parent()->objectName(), _pNormalState);
  _pFullSizeState = new StateDebug("fullSize", parent()->objectName(), _pNormalState);
  _pHideState = new StateDebug("hide", parent()->objectName(), _pNormalState);

  if(_mode == FULL) {
      _pNormalState->setInitialState(_pFullSizeState);
    }
  else if (_mode == SMALL) {
      _pNormalState->setInitialState(_pSmallSizeState);
    }
  else {
      qDebug() << "_mode inconnu dans TimeboxPresenter::createStates()";
    }

  _pStateMachine->addState(_pNormalState);

  _pFinalState = new QFinalState(); /// @todo gérer le final state et la suppression d'objets graphiques
  _pStateMachine->addState(_pFinalState);
}

void TimeboxPresenter::createTransitions()
{
  _pInitialState->addTransition(_pInitialState, SIGNAL(propertiesAssigned()), _pNormalState);

  if (_pSmallView != nullptr) {
      _pSmallSizeState->addTransition(_pSmallView, SIGNAL(headerDoubleClicked()), _pFullSizeState); /// User clicked on timeboxHeader (smallView)
    }
  _pSmallSizeState->addTransition(_pTimebox, SIGNAL(isHide()), _pHideState); /// Sister of the timebox passing from small to full

  _pFullSizeState->addTransition(_pTimebox, SIGNAL(isSmall()), _pSmallSizeState); /// User clicked on headerWidget. timebox's class is needed to route signal from the mouseClick from MainWindow::headerWidgetClicked()
  _pFullSizeState->addTransition(_pTimebox, SIGNAL(isHide()), _pHideState); /// His child go from small to full

  _pHideState->addTransition(_pTimebox, SIGNAL(isFull()), _pFullSizeState); /// His child go from full to small
  _pHideState->addTransition(_pTimebox, SIGNAL(isSmall()), _pSmallSizeState); /// Sister of the timebox passing from full to small

  if (_pSmallView != nullptr) {
      ///@bug cette méthode de suppression n'est plus utilisée (on passe par mainWindow::deleteSelectedItems())
      _pNormalState->addTransition(_pSmallView, SIGNAL(suppressTimebox()), _pFinalState); /// we only allow to suppress a timebox in SMALL mode
    }
}

void TimeboxPresenter::createConnections()
{
  connect(_pInitialState, SIGNAL(entered()), this, SLOT(init()));
  connect(_pSmallSizeState, SIGNAL(entered()), this, SLOT(goSmallView()));
  connect(_pFullSizeState, SIGNAL(entered()), this, SLOT(goFullView()));
  connect(_pStateMachine, SIGNAL(finished()), this, SIGNAL(suppressTimeboxProxy())); /// The finalState has been entered
}

void TimeboxPresenter::init() {
  if(_mode == FULL) {
      _pGraphicsView->setScene(_pFullView);
      _pGraphicsView->centerOn(QPointF(0,0));
      addStorey(ScenarioPluginType);
    }
  else if (_mode == SMALL) {
      addStorey(AutomationPluginType);
    }
}

void TimeboxPresenter::storeyBarButtonClicked(bool id)
{
  Q_ASSERT(id == 0 || id == 1);
  TimeboxStorey* tbs = qobject_cast<TimeboxStorey*>(sender());
  if(id == 0) { /// We add a new storey
      addStorey(AutomationPluginType);
      tbs->setButton(1);
    }
  else if(id == 1) {
      deleteStorey(tbs);
    }
}

void TimeboxPresenter::addStorey(int pluginType)
{
  PluginView *plugin;
  TimeboxStorey *pStorey;
  switch (_mode) {
    case SMALL:
      pStorey = new TimeboxStorey(_pModel, _pModel->width(), _pModel->height(), _pSmallView);
      pStorey->setButton(0);
      _pSmallView->addStorey(pStorey);
      plugin = addPlugin(pluginType, pStorey); /// We put the plugin in the storey
      _storeysSmallView.emplace(pStorey, plugin);
      _pModel->addPluginSmall();
      break;

    case FULL:
      pStorey = new TimeboxStorey(_pModel, _pModel->width(), _pModel->height(), _pFullView->container());
      //pStorey = new TimeboxStorey(_pModel, _pGraphicsView->width(), _pGraphicsView->height(), _pFullView->container());
      _pFullView->addStorey(pStorey);
      pStorey->setButton(0);
      plugin = addPlugin(pluginType, pStorey); /// We put the plugin in the storey
      _storeysFullView.emplace(pStorey, plugin);
      _pModel->addPluginFull();
      break;

    case HIDE:
      return;
    }

  connect(pStorey, SIGNAL(buttonClicked(bool)), this, SLOT(storeyBarButtonClicked(bool)));
}

PluginView * TimeboxPresenter::addPlugin(int pluginType, TimeboxStorey *pStorey)
{
  PluginView *plugin;
  switch(pluginType) {
    case ScenarioPluginType:
      { /// We have to put braces because we declare a new object in a switch statement
        ScenarioView *scenarioView = new ScenarioView(pStorey);

        /// We connect the plugin to proxies signals, to route them to the class Timebox
        connect(scenarioView, SIGNAL(createTimebox(QRectF)), this, SIGNAL(createBoxProxy(QRectF)));
        connect(scenarioView, SIGNAL(createTimeEventAction(QPointF)), this, SIGNAL(createTimeEventProxy(QPointF)));

        plugin = qgraphicsitem_cast<PluginView*>(scenarioView);
        break;
      }

    case AutomationPluginType:
      {
        AutomationView *automationView = new AutomationView(pStorey);
        plugin = qgraphicsitem_cast<PluginView*>(automationView);
        break;
      }

    }

  return plugin;
}

void TimeboxPresenter::goSmallView()
{
  _mode = SMALL;
}

void TimeboxPresenter::goFullView()
{
  _mode = FULL; /// mode need to be actualised first
  if (_pFullView == NULL) {
      createFullView();
    }
  _pGraphicsView->setScene(_pFullView);
  emit viewModeIsFull();
  //_pGraphicsView->fitFullView(); @todo Besoin d'etre amélioré
}

void TimeboxPresenter::goHide()
{
  _mode = HIDE;
}

void TimeboxPresenter::createFullView()
{
  _pFullView = new TimeboxFullView(_pModel, _pTimebox);
  std::list<TTTimeProcess*> lst = _pModel->pluginsFullView();

  /// @todo récupérer les plugins de smallsize

  for (std::list<TTTimeProcess*>::iterator it = lst.begin() ; it != lst.end() ; ++it) { /// On crée un storey par plugin
      addStorey(AutomationPluginType); ///@todo seul le dernier storey a un "+"
      /// @todo constructeur par copie des plugins pour aller dans full
    }
}

void TimeboxPresenter::deleteStorey(TimeboxStorey* tbs)
{
  std::unordered_map<TimeboxStorey*, PluginView*>::iterator it;

  switch(_mode) {
    case SMALL:
      it = _storeysSmallView.find(tbs);
      delete it->first; /// we delete the timeboxstorey
      _storeysSmallView.erase(it);
      _pModel->removePluginSmall(); /// @todo supp un storey ne veut pas dire supp son plugin. A changer le temps venu
      break;

    case FULL:
      it = _storeysFullView.find(tbs);
      delete it->first; /// we delete the timeboxstorey
      _storeysFullView.erase(it);
      _pModel->removePluginFull();
      break;

    case HIDE:
      break;
    }

}

