#pragma once
#include <QState>
#include <wobjectdefs.h>
#include <score/selection/SelectionDispatcher.hpp>
#include <score/statemachine/StateMachineTools.hpp>

class QGraphicsItem;

namespace score
{
class SelectionStack;
}

/**
 * @brief The CommonSelectionState class
 *
 * A generic state to handle traditional rectangular selection in a
 * QGraphicsScene.
 *
 * NOTE : the posted events must have the same id as Press / Move / Release
 * event,
 * e.g. PressOnNothing_Event, etc.
 */
class SCORE_LIB_BASE_EXPORT CommonSelectionState : public QState
{
  W_OBJECT(CommonSelectionState)
public:
  score::SelectionDispatcher dispatcher;

  virtual ~CommonSelectionState();

  virtual void on_pressAreaSelection() = 0;
  virtual void on_moveAreaSelection() = 0;
  virtual void on_releaseAreaSelection() = 0;
  virtual void on_deselect() = 0;
  virtual void on_delete() = 0;
  virtual void on_deleteContent() = 0;

  bool multiSelection() const;

protected:
  CommonSelectionState(
      score::SelectionStack& stack, QObject* process_view, QState* parent);

private:
  QState* m_waitState{};
};
