#pragma once
#include <Scenario/Commands/Interval/Rack/Slot/ResizeSlotVertically.hpp>
#include <Scenario/Commands/Interval/Rack/SwapSlots.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Interval/Slot.hpp>
#include <Scenario/Palette/ScenarioPaletteBaseStates.hpp>
#include <Scenario/Palette/ScenarioPaletteBaseTransitions.hpp>
#include <Scenario/Palette/Transitions/AnythingTransitions.hpp>

#include <score/command/Dispatchers/SingleOngoingCommandDispatcher.hpp>
#include <score/document/DocumentInterface.hpp>

#include <QFinalState>

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
        QEvent::Type(QEvent::User + MoveOnTimeSync_Event::user_type),
        QEvent::Type(QEvent::User + MoveOnInterval_Event::user_type),
        QEvent::Type(QEvent::User + MoveOnLeftBrace_Event::user_type),
        QEvent::Type(QEvent::User + MoveOnRightBrace_Event::user_type),
        QEvent::Type(QEvent::User + MoveOnSlotHandle_Event::user_type),
    };

    return find(begin(types), end(types), e->type()) != end(types);
  }
  void onTransition(QEvent* event) override { }
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
        QEvent::Type(QEvent::User + ReleaseOnTimeSync_Event::user_type),
        QEvent::Type(QEvent::User + ReleaseOnInterval_Event::user_type),
        QEvent::Type(QEvent::User + ReleaseOnLeftBrace_Event::user_type),
        QEvent::Type(QEvent::User + ReleaseOnRightBrace_Event::user_type),
        QEvent::Type(QEvent::User + ReleaseOnSlotHandle_Event::user_type),
    };

    return find(begin(types), end(types), e->type()) != end(types);
  }
  void onTransition(QEvent* event) override { }
};

template <typename Scenario_T, typename ToolPalette_T>
class ResizeSlotState final : public SlotState
{
public:
  ResizeSlotState(const score::CommandStackFacade& stack, const ToolPalette_T& sm, QState* parent)
      : SlotState{parent}, m_ongoingDispatcher{stack}, m_sm{sm}
  {
    auto press = new QState{this};
    this->setInitialState(press);
    auto move = new QState{this};
    auto release = new QFinalState{this};

    score::make_transition<Scenario::MoveOnAnything_SlotTransition>(press, move);
    score::make_transition<Scenario::MoveOnAnything_SlotTransition>(move, move);
    score::make_transition<Scenario::ReleaseOnAnything_SlotTransition>(press, release);
    score::make_transition<Scenario::ReleaseOnAnything_SlotTransition>(move, release);

    connect(press, &QAbstractState::entered, [=]() {
      m_originalPoint = m_sm.scenePoint;

      const IntervalModel& cst = this->currentSlot.interval.find(stack.context());
      m_originalHeight = cst.getSlotHeight(this->currentSlot);
    });

    connect(move, &QAbstractState::entered, [=]() {
      auto val = std::max(20.0, m_originalHeight + (m_sm.scenePoint.y() - m_originalPoint.y()));

      const IntervalModel& cst = this->currentSlot.interval.find(stack.context());
      m_ongoingDispatcher.submit(cst, this->currentSlot, val);
    });

    connect(release, &QAbstractState::entered, [=]() {
      m_ongoingDispatcher.commit();

      IntervalModel& cst = this->currentSlot.interval.find(stack.context());
      cst.heightFinishedChanging();
    });
  }

private:
  SingleOngoingCommandDispatcher<Scenario::Command::ResizeSlotVertically> m_ongoingDispatcher;
  const ToolPalette_T& m_sm;
};
}
