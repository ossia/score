#pragma once
#include <Scenario/Palette/ScenarioPaletteBaseStates.hpp>
#include <iscore/command/Dispatchers/SingleOngoingCommandDispatcher.hpp>

#include <Scenario/Commands/Constraint/Rack/Slot/ResizeSlotVertically.hpp>

#include <Scenario/Palette/ScenarioPaletteBaseTransitions.hpp>

#include <Scenario/Document/Constraint/Slot.hpp>

#include <Scenario/Commands/Constraint/Rack/SwapSlots.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Palette/Transitions/AnythingTransitions.hpp>
#include <iscore/document/DocumentInterface.hpp>

#include <QFinalState>
#include <QGraphicsScene>

namespace Scenario
{

class MoveOnAnything_SlotTransition final : public QAbstractTransition
{
protected:
  bool eventTest(QEvent* e) override
  {
    using namespace std;
    using namespace Scenario;
    static const constexpr QEvent::Type types[] = {
        QEvent::Type(QEvent::User + MoveOnNothing_Event::user_type),
        QEvent::Type(QEvent::User + MoveOnState_Event::user_type),
        QEvent::Type(QEvent::User + MoveOnEvent_Event::user_type),
        QEvent::Type(QEvent::User + MoveOnTimeNode_Event::user_type),
        QEvent::Type(QEvent::User + MoveOnConstraint_Event::user_type),
        QEvent::Type(QEvent::User + MoveOnLeftBrace_Event::user_type),
        QEvent::Type(QEvent::User + MoveOnRightBrace_Event::user_type),
        QEvent::Type(QEvent::User + MoveOnSlotHandle_Event::user_type),
    };

    return find(begin(types), end(types), e->type()) != end(types);
  }
  void onTransition(QEvent* event) override
  {
  }
};

class ReleaseOnAnything_SlotTransition final : public QAbstractTransition
{
protected:
  bool eventTest(QEvent* e) override
  {
    using namespace std;
    using namespace Scenario;
    static const constexpr QEvent::Type types[] = {
        QEvent::Type(QEvent::User + ReleaseOnNothing_Event::user_type),
        QEvent::Type(QEvent::User + ReleaseOnState_Event::user_type),
        QEvent::Type(QEvent::User + ReleaseOnEvent_Event::user_type),
        QEvent::Type(QEvent::User + ReleaseOnTimeNode_Event::user_type),
        QEvent::Type(QEvent::User + ReleaseOnConstraint_Event::user_type),
        QEvent::Type(QEvent::User + ReleaseOnLeftBrace_Event::user_type),
        QEvent::Type(QEvent::User + ReleaseOnRightBrace_Event::user_type),
        QEvent::Type(QEvent::User + ReleaseOnSlotHandle_Event::user_type),
    };

    return find(begin(types), end(types), e->type()) != end(types);
  }
  void onTransition(QEvent* event) override
  {
  }
};

template <typename Scenario_T, typename ToolPalette_T>
class ResizeSlotState final : public SlotState
{
public:
  ResizeSlotState(
      const iscore::CommandStackFacade& stack,
      const ToolPalette_T& sm,
      QState* parent)
      : SlotState{parent}, m_ongoingDispatcher{stack}, m_sm{sm}
  {
    auto press = new QState{this};
    this->setInitialState(press);
    auto move = new QState{this};
    auto release = new QFinalState{this};

    iscore::make_transition<Scenario::MoveOnAnything_SlotTransition>(
        press, move);
    iscore::make_transition<Scenario::MoveOnAnything_SlotTransition>(
        move, move);
    iscore::make_transition<Scenario::ReleaseOnAnything_SlotTransition>(
        press, release);
    iscore::make_transition<Scenario::ReleaseOnAnything_SlotTransition>(
        move, release);

    connect(press, &QAbstractState::entered, [=]() {
      qDebug() << "Pressed: " << this->currentSlot.index;
      m_originalPoint = m_sm.scenePoint;

      const ConstraintModel& cst = this->currentSlot.constraint.find();
      m_originalHeight = cst.getSlotHeight(this->currentSlot);
    });

    connect(move, &QAbstractState::entered, [=]() {
      auto val = std::max(
          20.0,
          m_originalHeight + (m_sm.scenePoint.y() - m_originalPoint.y()));

      m_ongoingDispatcher.submitCommand(this->currentSlot, val);
    });

    connect(release, &QAbstractState::entered, [=]() {
      qDebug() << "Released: " << this->currentSlot.index;
      m_ongoingDispatcher.commit();
    });
  }

private:
  SingleOngoingCommandDispatcher<Scenario::Command::ResizeSlotVertically>
      m_ongoingDispatcher;
  const ToolPalette_T& m_sm;
};
}
