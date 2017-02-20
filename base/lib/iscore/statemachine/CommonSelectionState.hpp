#pragma once
#include <QState>
#include <iscore/selection/SelectionDispatcher.hpp>
#include <iscore/statemachine/StateMachineTools.hpp>

class QQuickPaintedItem;

namespace iscore
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
class ISCORE_LIB_BASE_EXPORT CommonSelectionState : public QState
{
  Q_OBJECT
public:
  iscore::SelectionDispatcher dispatcher;

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
      iscore::SelectionStack& stack, QObject* process_view, QState* parent);

private:
  QState* m_waitState{};
};
