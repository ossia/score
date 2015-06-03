#pragma once
#include <QState>
#include <QFinalState>
#include <iscore/statemachine/StateMachineUtils.hpp>

// TODO rename file
class OngoingState : public QState
{
    public:
        template<typename CommandObject>
        OngoingState(CommandObject& obj, QState* parent)
        {
            QState* mainState = new QState{this};
            {
                auto pressed = new QState{mainState};
                auto moving = new QState{mainState};
                auto released = new QFinalState{mainState};

                // General setup
                mainState->setInitialState(pressed);

                make_transition<Move_Transition>(pressed, moving); // Also try with pressed, released
                make_transition<Release_Transition>(pressed, released);

                make_transition<Move_Transition>(moving, moving);
                make_transition<Release_Transition>(moving, released);

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

            setInitialState(mainState);
        }
};
