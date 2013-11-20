/*
Copyright: LaBRI / SCRIME

Authors : Jaime Chao, Clément Bossut (2013-2014)

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

#include "timeboxmodel.hpp"
#include <QStateMachine>
#include <QFinalState>
#include <QGraphicsScene>

TimeboxModel::TimeboxModel(int t, int y, int l, int h)
  : _time(t), _yPosition(y), _width(l), _height(h), _pluginsSmallView(1)
{

  createStates();
  createTransitions();
  _stateMachine->start();
}

TimeboxModel::~TimeboxModel()
{
  delete _initialState;
  delete _normalState; //will delete all child states
  delete _finalState;
}

void TimeboxModel::createStates()
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

  //if(parent == NULL) {
      _normalState->setInitialState(_fullSizeState);
   // }
 // else {
    //  _normalState->setInitialState(_smallSizeState);
   // }
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


void TimeboxModel::addPlugin()
{
  _pluginsSmallView.emplace_back();
}
