#include "graphicstimeprocess.hpp"
#include <QPainter>
#include <QGraphicsScene>
#include <QFinalState>
#include <QDebug>
#include <QTimer>

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
  painter->drawRect(0,0,m_width,20);

  painter->setPen(Qt::SolidLine);
  painter->setBrush(boxBrush());
  painter->drawText(boundingRect(), Qt::AlignLeft | Qt::AlignTop, objectName());

  /// Draw the bounding rectangle
  painter->drawRect(boundingRect());
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
