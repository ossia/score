#include "CreateEventStateMachine.hpp"
#include <Process/ScenarioModel.hpp>
#include <Document/TimeNode/TimeNodeModel.hpp>
#include <Document/Constraint/ConstraintModel.hpp>
#include <Document/Event/EventModel.hpp>

#include "Commands/Scenario/Creations/CreateEventAfterEvent.hpp"
#include <QFinalState>
CreateEventState::CreateEventState(iscore::CommandStack& stack, QState* parent):
    QState{parent},
    m_dispatcher{stack, nullptr}
{
    using namespace Scenario::Command;
    auto finalState = new QFinalState{this};
    QState* topState = new QState{this};
    {
        QState* pressedState = new QState{topState};
        QState* movingState = new QState{topState};
        QState* releasedState = new QState{topState};

        pressedState->addTransition(this, SIGNAL(move()), movingState);
        pressedState->addTransition(this, SIGNAL(release()), releasedState);
        movingState->addTransition(this, SIGNAL(release()), releasedState);
        movingState->addTransition(this, SIGNAL(move()), movingState);
        releasedState->addTransition(finalState);

        topState->setInitialState(pressedState);

        QObject::connect(pressedState, &QState::entered, [&] ()
        {
            auto init = new CreateEventAfterEvent{
                                ObjectPath{m_scenarioPath},
                                m_firstEvent,
                                m_eventDate,
                                m_ypos};
            m_createdEvent = init->createdEvent();

            m_dispatcher.submitCommand(init);
        });

        QObject::connect(movingState, &QState::entered, [&] ()
        {
            m_dispatcher.submitCommand(
                        new MoveEvent{
                                ObjectPath{m_scenarioPath},
                                m_createdEvent,
                                m_eventDate,
                                m_ypos});
        });

        QObject::connect(releasedState, &QState::entered, [&] ()
        {
            m_dispatcher.commit();
        });
    }

    QState* rollbackState = new QState{this};
    topState->addTransition(this, SIGNAL(cancel()), rollbackState);
    rollbackState->addTransition(finalState);
    QObject::connect(rollbackState, &QState::entered, [&] ()
    {
        m_dispatcher.rollback();
    });

    setInitialState(topState);

}

void CreateEventState::init(ObjectPath&& path,
                                   id_type<EventModel> startEvent,
                                   const TimeValue& date,
                                   double y)
{
    m_scenarioPath = std::move(path);
    m_firstEvent = startEvent;
    m_eventDate = date;
    m_ypos = y;

    emit init();
}
