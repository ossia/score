#pragma once
#include <QState>
#include <QFinalState>
#include <iscore/statemachine/StateMachineUtils.hpp>
#include "Curve/StateMachine/CurveStateMachineBaseTransitions.hpp"
#include "Curve/StateMachine/CurvePoint.hpp"


namespace Curve
{
class OngoingState : public Curve::StateBase
{
    public:
        template<typename CommandObject>
        OngoingState(CommandObject& obj, QState* parent):
            Curve::StateBase{parent}
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
                make_transition<MoveOnAnything_Transition>(pressed, moving, *this);
                make_transition<ReleaseOnAnything_Transition>(pressed, released);

                make_transition<MoveOnAnything_Transition>(moving, moving, *this);
                make_transition<ReleaseOnAnything_Transition>(moving, released);

                connect(pressed, &QAbstractState::entered,
                        this, [&] () {
                    obj.press();
                });
                connect(moving, &QAbstractState::entered,
                        this, [&] () {
                    obj.move();
                });
                connect(released, &QAbstractState::entered,
                        this, [&] () {
                    obj.release();
                });
            }

            mainState->addTransition(mainState, SIGNAL(finished()), finalState);

            auto cancelled = new QState{this};
            make_transition<Cancel_Transition>(mainState, cancelled);
            cancelled->addTransition(finalState);

            connect(cancelled, &QAbstractState::entered,
                    this, [&] () {
                obj.cancel();
            });

        }
};
}
