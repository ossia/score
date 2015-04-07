#include "MoveStates.hpp"

#include <Process/ScenarioModel.hpp>
#include <Document/TimeNode/TimeNodeModel.hpp>
#include <Document/Constraint/ConstraintModel.hpp>
#include <Document/Event/EventModel.hpp>

#include "Commands/Scenario/Displacement/MoveEvent.hpp"
#include "Commands/Scenario/Displacement/MoveTimeNode.hpp"
#include "Commands/Scenario/Displacement/MoveConstraint.hpp"

#include <QFinalState>

MoveConstraintState::MoveConstraintState(ObjectPath&& scenarioPath,
                                         iscore::CommandStack& stack,
                                         iscore::ObjectLocker& locker,
                                         QState* parent):
    CommonState{ObjectPath{scenarioPath}, parent},
    m_dispatcher{std::move(scenarioPath), locker, stack, this}
{
    using namespace Scenario::Command ;
    auto finalState = new QFinalState{this};

    QState* mainState = new QState{this};
    {
        QState* pressed = new QState{mainState};
        QState* released = new QState{mainState};
        QState* moving = new QState{mainState};

        // General setup
        mainState->setInitialState(pressed);
        released->addTransition(finalState);

        auto t_pressed =
                make_transition<MoveOnAnything_Transition>(
                    pressed, moving , *this);
        connect(t_pressed, &QAbstractTransition::triggered, [&] ()
        {
            auto scenar = m_scenarioPath.find<ScenarioModel>();
            m_constraintInitialStartDate= scenar->constraint(clickedConstraint)->startDate();
            m_constraintInitialClickDate = point.date;
        });

        make_transition<ReleaseOnAnything_Transition>(
                    pressed, finalState);
        make_transition<MoveOnAnything_Transition>(
                    moving , moving , *this);
        make_transition<ReleaseOnAnything_Transition>(
                    moving , released);

        QObject::connect(moving, &QState::entered, [&] ()
        {
            m_dispatcher.submitCommand(
                        new MoveConstraint{
                            ObjectPath{m_scenarioPath},
                            clickedConstraint,
                            m_constraintInitialStartDate + (point.date - m_constraintInitialClickDate),
                            point.y});
        });

        QObject::connect(released, &QState::entered, [&] ()
        {
            m_dispatcher.commit();
        });
    }

    QState* rollbackState = new QState{this};
    make_transition<Cancel_Transition>(mainState, rollbackState);
    rollbackState->addTransition(finalState);
    QObject::connect(rollbackState, &QState::entered, [&] ()
    {
        m_dispatcher.rollback();
    });

    setInitialState(mainState);
}


MoveEventState::MoveEventState(ObjectPath&& scenarioPath, iscore::CommandStack& stack, iscore::ObjectLocker& locker, QState* parent):
    CommonState{ObjectPath{scenarioPath}, parent},
    m_dispatcher{std::move(scenarioPath), locker, stack, this}
{
    using namespace Scenario::Command ;
    auto finalState = new QFinalState{this};

    QState* mainState = new QState{this};
    {
        QState* pressed = new QState{mainState};
        QState* released = new QState{mainState};
        QState* moving = new QState{mainState};

        // General setup
        mainState->setInitialState(pressed);
        released->addTransition(finalState);

        make_transition<MoveOnAnything_Transition>(
                    pressed, moving, *this);
        make_transition<ReleaseOnAnything_Transition>(
                    pressed, finalState);
        make_transition<MoveOnAnything_Transition>(
                    moving, moving, *this);
        make_transition<ReleaseOnAnything_Transition>(
                    moving, released);

        // What happens in each state.
        QObject::connect(moving, &QState::entered, [&] ()
        {
            m_dispatcher.submitCommand(
                        new MoveEvent{
                            ObjectPath{m_scenarioPath},
                            clickedEvent,
                            point.date,
                            point.y});
        });

        QObject::connect(released, &QState::entered, [&] ()
        {
            m_dispatcher.commit();
        });
    }

    QState* rollbackState = new QState{this};
    make_transition<Cancel_Transition>(mainState, rollbackState);
    rollbackState->addTransition(finalState);
    QObject::connect(rollbackState, &QState::entered, [&] ()
    {
        m_dispatcher.rollback();
    });

    setInitialState(mainState);
}


MoveTimeNodeState::MoveTimeNodeState(ObjectPath&& scenarioPath, iscore::CommandStack& stack, iscore::ObjectLocker& locker, QState* parent):
    CommonState{ObjectPath{scenarioPath}, parent},
    m_dispatcher{std::move(scenarioPath), locker, stack, this}
{
    using namespace Scenario::Command ;
    auto finalState = new QFinalState{this};

    QState* mainState = new QState{this};
    {
        QState* pressed = new QState{mainState};
        QState* released = new QState{mainState};
        QState* moving = new QState{mainState};
        mainState->setInitialState(pressed);

        // General setup
        released->addTransition(finalState);

        make_transition<MoveOnAnything_Transition>(
                    pressed, moving, *this);
        make_transition<ReleaseOnAnything_Transition>(
                    pressed, finalState);
        make_transition<MoveOnAnything_Transition>(
                    moving, moving, *this);
        make_transition<ReleaseOnAnything_Transition>(
                    moving, released);

        // What happens in each state.
        QObject::connect(moving, &QState::entered, [&] ()
        {
            // Get the 1st event on the timenode.
            auto scenar = m_scenarioPath.find<ScenarioModel>();
            auto tn = scenar->timeNode(clickedTimeNode);
            auto ev_id = tn->events().first();


            m_dispatcher.submitCommand(
                        new MoveTimeNode{
                            ObjectPath{m_scenarioPath},
                            ev_id,
                            point.date,
                            scenar->event(ev_id)->heightPercentage()});
        });

        QObject::connect(released, &QState::entered, [&] ()
        {
            m_dispatcher.commit();
        });
    }

    QState* rollbackState = new QState{this};
    make_transition<Cancel_Transition>(mainState, rollbackState);
    rollbackState->addTransition(finalState);
    QObject::connect(rollbackState, &QState::entered, [&] ()
    {
        m_dispatcher.rollback();
    });

    setInitialState(mainState);
}
