#pragma once
#include <Curve/Palette/CurvePaletteBaseTransitions.hpp>
#include <Curve/Palette/CurvePoint.hpp>

#include <score/statemachine/StateMachineTools.hpp>
#include <score/statemachine/StateMachineUtils.hpp>

#include <QFinalState>
#include <QState>

namespace Curve
{
class OngoingState final : public Curve::StateBase
{
public:
  template <typename CommandObject>
  OngoingState(CommandObject& obj, QState* parent) : Curve::StateBase{parent}
  {
    using namespace Curve;

    obj.setCurveState(this);

    auto mainState = new QState{this};
    auto finalState = new QFinalState{this};

    setInitialState(mainState);
    {
      auto pressed = new QState{mainState};
      pressed->setObjectName("Pressed");
      auto moving = new QState{mainState};
      moving->setObjectName("Moving");
      auto released = new QFinalState{mainState};
      released->setObjectName("Released");

      // General setup
      mainState->setInitialState(pressed);

      // Also try with pressed, released
      score::make_transition<MoveOnAnything_Transition>(pressed, moving, *this);
      score::make_transition<ReleaseOnAnything_Transition>(pressed, released);

      score::make_transition<MoveOnAnything_Transition>(moving, moving, *this);
      score::make_transition<ReleaseOnAnything_Transition>(moving, released);

      connect(pressed, &QAbstractState::entered, this, [&]() { obj.press(); });
      connect(moving, &QAbstractState::entered, this, [&]() { obj.move(); });
      connect(released, &QAbstractState::entered, this, [&]() { obj.release(); });
    }

    mainState->addTransition(mainState, finishedState(), finalState);

    auto cancelled = new QState{this};
    score::make_transition<score::Cancel_Transition>(mainState, cancelled);
    cancelled->addTransition(finalState);

    connect(cancelled, &QAbstractState::entered, this, [&]() { obj.cancel(); });
  }
};
}
