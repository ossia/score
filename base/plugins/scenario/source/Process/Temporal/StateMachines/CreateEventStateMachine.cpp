#include "CreateEventStateMachine.hpp"
#include <Process/ScenarioModel.hpp>
#include <Document/TimeNode/TimeNodeModel.hpp>
#include <Document/Constraint/ConstraintModel.hpp>
#include <Document/Event/EventModel.hpp>

#include "Commands/Scenario/Creations/CreateEventAfterEvent.hpp"
#include <QFinalState>
class PressedState : public QState
{
    public:
        using QState::QState;
};
class MovingState : public QState
{
    public:
        using QState::QState;
};
class ReleasedState : public QState
{
    public:
        using QState::QState;
};

CreateEventState::CreateEventState(ObjectPath &&scenarioPath, iscore::CommandStack& stack, QState* parent):
    QState{parent},
    m_scenarioPath{std::move(scenarioPath)},
    m_dispatcher{stack, nullptr}
{
    using namespace Scenario::Command;
    auto finalState = new QFinalState{this};
    QState* mainState = new QState{this};
    {
        QState* pressedState = new PressedState{mainState};
        QState* movingState = new MovingState{mainState};
        QState* releasedState = new ReleasedState{mainState};

        auto tr1 = new ScenarioRelease_Transition;
        tr1->setTargetState(releasedState);
        pressedState->addTransition(tr1);
        auto tr2 = new ScenarioRelease_Transition;
        tr2->setTargetState(releasedState);
        movingState->addTransition(tr2);

        pressedState->addTransition(this, SIGNAL(move()), movingState);
        movingState->addTransition(this, SIGNAL(move()), movingState);
        releasedState->addTransition(finalState);

        mainState->setInitialState(pressedState);

        QObject::connect(pressedState, &QState::entered, [&] ()
        {
            qDebug() << "Badaboum";
            auto init = new CreateEventAfterEvent{
                                ObjectPath{m_scenarioPath},
                                firstEvent,
                                eventDate,
                                ypos};
            createdEvent = init->createdEvent();

            m_dispatcher.submitCommand(init);
        });

        QObject::connect(movingState, &QState::entered, [&] ()
        {
            qDebug() << "move";
            m_dispatcher.submitCommand(
                        new MoveEvent{
                                ObjectPath{m_scenarioPath},
                                createdEvent,
                                eventDate,
                                ypos});
        });

        QObject::connect(releasedState, &QState::entered, [&] ()
        {
            qDebug() << "exit";
            m_dispatcher.commit();
        });
    }

    QState* rollbackState = new QState{this};
    mainState->addTransition(this, SIGNAL(cancel()), rollbackState);
    rollbackState->addTransition(finalState);
    QObject::connect(rollbackState, &QState::entered, [&] ()
    {
        m_dispatcher.rollback();
    });

    setInitialState(mainState);
}

