#include "CreateEventStateMachine.hpp"
#include <Process/ScenarioModel.hpp>
#include <Document/TimeNode/TimeNodeModel.hpp>
#include <Document/Constraint/ConstraintModel.hpp>
#include <Document/Event/EventModel.hpp>
// NOTE : macro commands should be able to share some data  ?
// Or have some mini-command that just do one thing (with undo() redo()) and operatoe on the data of the parent ?
// like :

/**
 *
 * Constraint - Event - Timenode
 *
 *
*/


class CreateConstraintMin
{
    public:
        template<typename Command>
        void undo(Command& cmd)
        {
            // Voire pareil pour time node; event.
            // Cf. feuilles dans sec.
        }

        template<typename Command>
        void redo(Command& cmd)
        {

        }
};







CreateEventStateMachine::CreateEventStateMachine(iscore::CommandStack& stack):
    m_dispatcher{stack, nullptr}
{
    ////////////
    ///
    ///
    ///
    ///
    QState* exitState = new QState;
    QState* topState = new QState;
    {
        QState* pressedState = new QState{topState};
        QState* movingState = new QState{topState};
        QState* releasedState = new QState{topState};

        pressedState->addTransition(this, SIGNAL(move()), movingState);
        pressedState->addTransition(this, SIGNAL(release()), releasedState);
        movingState->addTransition(this, SIGNAL(release()), releasedState);
        movingState->addTransition(this, SIGNAL(move()), movingState);

        topState->setInitialState(pressedState);

        // TODO QObject::connect(pressedState, &QState::entered, [&] ()
        //{ m_dispatcher.submitCommand(new Scenario::Command::CreateEvent()); });

        QObject::connect(movingState, &QState::entered, [&] ()
        { m_dispatcher.submitCommand(new Scenario::Command::MoveEvent()); });
    }

    topState->addTransition(this, SIGNAL(cancel()), exitState);
    m_sm.addState(topState);
    m_sm.addState(exitState);

    m_sm.setInitialState(topState);


    QObject::connect(exitState, &QState::entered, [&] () { m_dispatcher.rollback(); });

    m_sm.start();
}
