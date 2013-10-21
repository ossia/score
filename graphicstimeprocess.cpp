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

#include "graphicstimeprocess.hpp"
#include <QPainter>
#include <QGraphicsScene>
#include <QFinalState>
#include <QDebug>
#include <QTimer>
#include <QGraphicsSceneMouseEvent>

/// @todo Use a namespace ?
const qint32 headerHeight = 20;

GraphicsTimeProcess::GraphicsTimeProcess(const QPointF &position, QGraphicsItem *parent, QGraphicsScene *scene)
  : QGraphicsObject(parent), _scene(scene), m_boxEditingBrush(Qt::NoBrush), m_boxExecutionBrush(Qt::yellow, Qt::Dense6Pattern)
{
  setFlags(QGraphicsItem::ItemIsSelectable |
           QGraphicsItem::ItemIsMovable |
           QGraphicsItem::ItemSendsGeometryChanges);

  //creer les time event de début et de fin

  createStates(position, parent);
  createTransitions();

  stateMachine.setInitialState(initialState);
  stateMachine.start();

  _scene->clearSelection();
  _scene->addItem(this);
  setSelected(true);
}

GraphicsTimeProcess::~GraphicsTimeProcess()
{
  delete initialState;
  delete normalState; //will delete all child states
  delete finalState;
}

void GraphicsTimeProcess::createStates(const QPointF &position, QGraphicsItem *parent)
{
  initialState = new QState();
  initialState->assignProperty(this, "pos", position);
  initialState->assignProperty(this, "mainScenario", parent ? false : true); /// \todo peut mieux faire, utiliser propriété parent
  initialState->assignProperty(this, "objectName", tr("Box"));
  initialState->assignProperty(this, "running", false);
  initialState->assignProperty(this, "paused", false);
  initialState->assignProperty(this, "stopped", false);
  initialState->assignProperty(this, "height", 100);
  initialState->assignProperty(this, "width", 200);
  stateMachine.addState(initialState);

  // creating a new top-level state
  normalState = new QState();

  editionState = new QState(normalState);
  editionState->assignProperty(this,"enabled", true); /// @todo enabled n'est pas la plus approprié car trop radicale
  editionState->assignProperty(this, "boxBrush", m_boxEditingBrush);

  executionState = new QState(normalState);
  executionState->assignProperty(this,"enabled", false);
  executionState->assignProperty(this, "boxBrush", m_boxExecutionBrush);

  runningState = new QState(executionState);
  runningState->assignProperty(this, "running", true);
  runningState->assignProperty(this, "paused", false);
  runningState->assignProperty(this, "stopped", false);
  pausedState = new QState(executionState);
  pausedState->assignProperty(this, "running", false);
  pausedState->assignProperty(this, "paused", true);
  pausedState->assignProperty(this, "stopped", false);
  stoppedState = new QState(executionState);
  stoppedState->assignProperty(this, "running", false);
  stoppedState->assignProperty(this, "paused", false);
  stoppedState->assignProperty(this, "stopped", true);
  executionState->setInitialState(runningState);

  normalState->setInitialState(editionState);
  stateMachine.addState(normalState);

  //normalSizeState = new QState(editingState); //Using parrallel states to add normal and extended
  //extendedSizeState = new QState(editingState);

  finalState = new QFinalState(); /// @todo gérer le final state et la suppression d'objets graphiques
  stateMachine.addState(finalState);
}

void GraphicsTimeProcess::createTransitions()
{
  initialState->addTransition(initialState, SIGNAL(propertiesAssigned()), normalState);
  editionState->addTransition(this, SIGNAL(playOrPauseButtonClicked()), runningState);
  runningState->addTransition(this, SIGNAL(playOrPauseButtonClicked()), pausedState);
  runningState->addTransition(this, SIGNAL(stopButtonClicked()), stoppedState);
  pausedState->addTransition(this, SIGNAL(playOrPauseButtonClicked()), runningState);
  pausedState->addTransition(this, SIGNAL(stopButtonClicked()), stoppedState);
  stoppedState->addTransition(stoppedState, SIGNAL(propertiesAssigned()), editionState);
  normalState->addTransition(this, SIGNAL(suppress()), finalState);
}

QRectF GraphicsTimeProcess::boundingRect() const
{
  return QRectF(0,0,m_width,m_height);
}

void GraphicsTimeProcess::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
  Q_UNUSED(option)
  Q_UNUSED(widget)

  /// Draw the header part
  painter->setPen(Qt::NoPen);
  painter->setBrush(QBrush(Qt::gray));
  painter->drawRect(0,0,m_width, headerHeight);

  painter->setPen(Qt::SolidLine);
  painter->setBrush(boxBrush());
  painter->drawText(boundingRect(), Qt::AlignLeft | Qt::AlignTop, objectName());

  /// Draw the bounding rectangle
  painter->drawRect(boundingRect());
}

void GraphicsTimeProcess::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
  if (event->button() == Qt::LeftButton) {
      if (event->pos().y() <= headerHeight){
          emit headerClicked();
        }
    }
  QGraphicsObject::mouseDoubleClickEvent(event);
}

void GraphicsTimeProcess::setmainScenario(bool arg)
{
  if (m_mainScenario != arg) {
      m_mainScenario = arg;
      emit mainScenarioChanged(arg);
    }
}

void GraphicsTimeProcess::setwidth(qreal arg)
{
  if (m_width != arg) {
      m_width = arg;
      emit widthChanged(arg);
    }
}

void GraphicsTimeProcess::setheight(qreal arg)
{
  if (m_height != arg) {
      m_height = arg;
      emit heightChanged(arg);
    }
}

void GraphicsTimeProcess::setrunning(bool arg)
{
  if (m_running != arg) {
      m_running = arg;
      emit runningChanged(arg);
    }
}

void GraphicsTimeProcess::setpaused(bool arg)
{
  if (m_paused != arg) {
      m_paused = arg;
      emit pausedChanged(arg);
    }
}

void GraphicsTimeProcess::setstopped(bool arg)
{
  if (m_stopped != arg) {
      m_stopped = arg;
      emit stoppedChanged(arg);
    }
}

void GraphicsTimeProcess::setboxBrush(QBrush arg)
{
  if (m_boxBrush != arg) {
      m_boxBrush = arg;
      emit boxBrushChanged(arg);
    }
}
