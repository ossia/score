// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "CommonSelectionState.hpp"

#include "StateMachineUtils.hpp"

#include <QAbstractState>
#include <QAbstractTransition>
#include <QApplication>
#include <QFinalState>
#include <QGraphicsItem>
#include <QKeyEventTransition>
#include <qcoreevent.h>
#include <qnamespace.h>
#include <wobjectimpl.h>
W_OBJECT_IMPL(CommonSelectionState)

bool CommonSelectionState::multiSelection() const
{
  return qApp->queryKeyboardModifiers().testFlag(
      Qt::KeyboardModifier::ControlModifier);
}

CommonSelectionState::CommonSelectionState(
    score::SelectionStack& stack, QObject* obj, QState* parent)
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

  score::make_transition<score::Press_Transition>(
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
    score::make_transition<score::Move_Transition>(
        pressAreaSelection, moveAreaSelection);
    score::make_transition<score::Release_Transition>(
        pressAreaSelection, releaseAreaSelection);

    score::make_transition<score::Move_Transition>(
        moveAreaSelection, moveAreaSelection);
    score::make_transition<score::Release_Transition>(
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
  score::make_transition<score::Cancel_Transition>(
      selectionAreaState, deselectState);
  score::make_transition<score::Cancel_Transition>(m_waitState, deselectState);
  score::make_transition<score::Cancel_Transition>(this, deselectState);
  deselectState->addTransition(m_waitState);
  connect(
      deselectState, &QAbstractState::entered, this,
      &CommonSelectionState::on_deselect);
}

CommonSelectionState::~CommonSelectionState() = default;
