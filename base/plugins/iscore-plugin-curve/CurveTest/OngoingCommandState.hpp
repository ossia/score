#pragma once
#include <QState>
#include <QFinalState>
#include <iscore/statemachine/StateMachineUtils.hpp>
#include "CurveTest/StateMachine/CurveStateMachineBaseTransitions.hpp"
#include "CurveTest/StateMachine/CurvePoint.hpp"


// TODO rename file
template<typename Element>
class OngoingState : public Curve::StateBase
{
    public:
        template<typename CommandObject>
        OngoingState(CommandObject& obj, QState* parent)
        {
            using namespace Curve;
            auto mainState = new QState{this};
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
                make_transition<PositionedCurveTransition<Element, Modifier::Move_tag>>(pressed, moving, *this);
                make_transition<PositionedCurveTransition<Element, Modifier::Release_tag>>(pressed, released, *this);

                make_transition<PositionedCurveTransition<Element, Modifier::Move_tag>>(moving, moving, *this);
                make_transition<PositionedCurveTransition<Element, Modifier::Release_tag>>(moving, released, *this);

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

            auto cancelled = new QState{this};
            auto finalState = new QFinalState{this};

            make_transition<Cancel_Transition>(mainState, cancelled);
            cancelled->addTransition(finalState);

            connect(cancelled, &QAbstractState::entered,
                    this, [&] () {
                obj.cancel();
            });

        }
};
