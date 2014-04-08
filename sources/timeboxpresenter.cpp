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

#include <QDebug>
#include <QGraphicsRectItem>
#include <QStateMachine>

TimeboxPresenter::TimeboxPresenter(TimeboxModel *pModel, TimeboxSmallView *pSmallView, GraphicsView *pView)
  : _mode(SMALL), _pModel(pModel), _pSmallView(pSmallView), _pFullView(NULL), _pGraphicsView(pView)
{  
  createStateMachine();

  addStorey(AutomationPluginType);

  connect(_pSmallView, SIGNAL(headerDoubleClicked()), this, SLOT(goFullView()));
  connect(_pSmallView, SIGNAL(suppressTimebox()), this, SIGNAL(suppressTimeboxProxy()));
}

TimeboxPresenter::TimeboxPresenter(TimeboxModel *pModel, TimeboxFullView *pFullView, GraphicsView *pView)
  : _mode(FULL), _pModel(pModel), _pSmallView(NULL), _pFullView(pFullView), _pGraphicsView(pView)
{
  createStateMachine();

  _pGraphicsView->setScene(_pFullView);
  _pGraphicsView->centerOn(QPointF(0,0));

  addStorey(ScenarioPluginType);
}

TimeboxPresenter::~TimeboxPresenter()
{
delete _initialState;
delete _normalState; //will delete all child states
delete _finalState;
}

void TimeboxPresenter::createStateMachine()
{
createStates();
createTransitions();
_stateMachine->start();
}

void TimeboxPresenter::createStates()
{
_stateMachine = new QStateMachine(this);

_initialState = new QState();
_initialState->assignProperty(this, "objectName", tr("Box"));
_stateMachine->addState(_initialState);
_stateMachine->setInitialState(_initialState);

// creating a new top-level state
_normalState = new QState();

_smallSizeState = new QState(_normalState);
_fullSizeState = new QState(_normalState);

if(_mode == FULL) {
    _normalState->setInitialState(_fullSizeState);
  }
else if (_mode == SMALL) {
    _normalState->setInitialState(_smallSizeState);
  }
else {
    qDebug() << "_mode inconnu dans TimeboxPresenter::createStates()";
  }

_stateMachine->addState(_normalState);

_finalState = new QFinalState(); /// @todo gérer le final state et la suppression d'objets graphiques
_stateMachine->addState(_finalState);
}

void TimeboxModel::createTransitions()
{
_initialState->addTransition(_initialState, SIGNAL(propertiesAssigned()), _normalState);
_fullSizeState->addTransition(this, SIGNAL(timeboxHeaderClicked()), _smallSizeState);
_smallSizeState->addTransition(this, SIGNAL(timeboxHeaderClicked()), _fullSizeState);
_normalState->addTransition(this, SIGNAL(suppress()), _finalState);
}

void TimeboxModel::createConnections()
{
//connect(_initialState, SIGNAL(entered()), this, SLOT(init()));
connect(_smallSizeState, SIGNAL(exited()), this, SLOT(switchToFullView)); /// @todo What happen if we exit to finalState ?
connect(_fullSizeState, SIGNAL(exited()), this, SLOT(switchToSmallView));
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
    }

  connect(pStorey, SIGNAL(buttonClicked(bool)), this, SLOT(storeyBarButtonClicked(bool)));
}

PluginView * TimeboxPresenter::addPlugin(int pluginType, TimeboxStorey *pStorey)
{
  PluginView *plugin;
  switch(pluginType) {
    case ScenarioPluginType:
      { /// We have to put braces because we declare a new object
        ScenarioView *scenarioView = new ScenarioView(pStorey);
        connect(scenarioView, SIGNAL(addTimebox(QGraphicsRectItem*)), this, SIGNAL(addBoxProxy(QGraphicsRectItem*))); /// We connect the plugin to a Proxy signal to route it to the class Timebox
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

void TimeboxPresenter::createFullView()
{
  _pFullView = new TimeboxFullView(_pModel);
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
    case SMALL :
      it = _storeysSmallView.find(tbs);
      delete it->first; /// we delete the timeboxstorey
      _storeysSmallView.erase(it);
      _pModel->removePluginSmall(); /// @todo supp un storey ne veut pas dire supp son plugin. A changer le temps venu
      break;

    case FULL :
      it = _storeysFullView.find(tbs);
      delete it->first; /// we delete the timeboxstorey
      _storeysFullView.erase(it);
      _pModel->removePluginFull();
      break;
    }

}

