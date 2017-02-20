#include <QAbstractState>
#include <QAbstractTransition>
#include <QFinalState>
#include <QQuickPaintedItem>
#include <QKeyEventTransition>
#include <qcoreevent.h>
#include <qnamespace.h>

#include "CommonSelectionState.hpp"
#include "StateMachineUtils.hpp"
#include <QApplication>

bool CommonSelectionState::multiSelection() const
{
  return qApp->queryKeyboardModifiers().testFlag(
      Qt::KeyboardModifier::ControlModifier);
}

CommonSelectionState::CommonSelectionState(
    iscore::SelectionStack& stack, QObject* obj, QState* parent)
    : QState{parent}, dispatcher{stack}
{
  setObjectName("metaSelectionState");

  // Wait
  m_waitState = new QState{this};
  m_waitState->setObjectName("m_waitState");
  this->setInitialState(m_waitState);

  // Area
  auto selectionAreaState = new QState{this};
  selectionAreaState->setObjectName("selectionAreaState");

  iscore::make_transition<iscore::Press_Transition>(
      m_waitState, selectionAreaState);
  selectionAreaState->addTransition(
      selectionAreaState, finishedState(), m_waitState);
  {
    // States
    auto pressAreaSelection = new QState{selectionAreaState};
    pressAreaSelection->setObjectName("pressAreaSelection");
    selectionAreaState->setInitialState(pressAreaSelection);
    auto moveAreaSelection = new QState{selectionAreaState};
    moveAreaSelection->setObjectName("moveAreaSelection");
    auto releaseAreaSelection = new QFinalState{selectionAreaState};
    releaseAreaSelection->setObjectName("releaseAreaSelection");

    // Transitions
    iscore::make_transition<iscore::Move_Transition>(
        pressAreaSelection, moveAreaSelection);
    iscore::make_transition<iscore::Release_Transition>(
        pressAreaSelection, releaseAreaSelection);

    iscore::make_transition<iscore::Move_Transition>(
        moveAreaSelection, moveAreaSelection);
    iscore::make_transition<iscore::Release_Transition>(
        moveAreaSelection, releaseAreaSelection);

    // Operations
    connect(
        pressAreaSelection, &QState::entered, this,
        &CommonSelectionState::on_pressAreaSelection);
    connect(
        moveAreaSelection, &QState::entered, this,
        &CommonSelectionState::on_moveAreaSelection);
    connect(
        releaseAreaSelection, &QState::entered, this,
        &CommonSelectionState::on_releaseAreaSelection);
  }

  // Deselection
  auto deselectState = new QState{this};
  deselectState->setObjectName("deselectState");
  iscore::make_transition<iscore::Cancel_Transition>(
      selectionAreaState, deselectState);
  iscore::make_transition<iscore::Cancel_Transition>(
      m_waitState, deselectState);
  iscore::make_transition<iscore::Cancel_Transition>(this, deselectState);
  deselectState->addTransition(m_waitState);
  connect(
      deselectState, &QAbstractState::entered, this,
      &CommonSelectionState::on_deselect);

  // Actions on selected elements
  // NOTE : see ObjectMenuActions too.
  auto t_delete = new QKeyEventTransition(
      obj, QEvent::KeyPress, Qt::Key_Backspace, m_waitState);
  connect(
      t_delete, &QAbstractTransition::triggered, this,
      &CommonSelectionState::on_delete);

  auto t_deleteContent = new QKeyEventTransition(
      obj, QEvent::KeyPress, Qt::Key_Delete, m_waitState);
  connect(
      t_deleteContent, &QAbstractTransition::triggered, this,
      &CommonSelectionState::on_deleteContent);
}

CommonSelectionState::~CommonSelectionState() = default;
