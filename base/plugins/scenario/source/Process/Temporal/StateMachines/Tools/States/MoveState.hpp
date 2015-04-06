#pragma once
#include <iscore/command/OngoingCommandManager.hpp>
#include "Process/Temporal/StateMachines/StateMachineCommon.hpp"


#include <Process/ScenarioModel.hpp>
#include <Document/TimeNode/TimeNodeModel.hpp>
#include <Document/Constraint/ConstraintModel.hpp>
#include <Document/Event/EventModel.hpp>

#include "Commands/Scenario/Displacement/MoveEvent.hpp"
#include "Commands/Scenario/Displacement/MoveTimeNode.hpp"
#include "Commands/Scenario/Displacement/MoveConstraint.hpp"

#include <QFinalState>



class MoveConstraintState : public CommonState
{
    public:
        MoveConstraintState(ObjectPath&& scenarioPath,
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
                QState* pressedConstraintState = new QState{mainState};
                QState* releasedState = new QState{mainState};
                QState* movingConstraint = new QState{mainState};

                // General setup
                mainState->setInitialState(pressedConstraintState);
                releasedState->addTransition(finalState);

                // Pressed -> ...
                auto t_pressed_constraint = new MoveOnAnything_Transition{*this};
                t_pressed_constraint->setTargetState(movingConstraint);
                pressedConstraintState->addTransition(t_pressed_constraint);

                // Moving -> ...
                auto t_moving_constraint = new MoveOnAnything_Transition{*this};
                t_moving_constraint->setTargetState(movingConstraint);
                movingConstraint->addTransition(t_moving_constraint);

                // Release
                auto t_release = new ReleaseOnAnything_Transition;
                t_release->setTargetState(releasedState);
                mainState->addTransition(t_release);


                QObject::connect(movingConstraint , &QState::entered, [&] ()
                {
                    m_dispatcher.submitCommand(
                                new MoveConstraint{
                                    ObjectPath{m_scenarioPath},
                                    clickedConstraint,
                                    point.date,
                                    point.y});
                });

                QObject::connect(releasedState, &QState::entered, [&] ()
                {
                    m_dispatcher.commit();
                });
            }

            QState* rollbackState = new QState{this};
            // TODO use event instead.
            // mainState->addTransition(this, SIGNAL(cancel()), rollbackState);
            rollbackState->addTransition(finalState);
            QObject::connect(rollbackState, &QState::entered, [&] ()
            {
                m_dispatcher.rollback();
            });

            setInitialState(mainState);
        }

        LockingOngoingCommandDispatcher<MergeStrategy::Simple, CommitStrategy::Redo> m_dispatcher;
};



class MoveEventState : public CommonState
{
    public:
        MoveEventState(ObjectPath&& scenarioPath,
                  iscore::CommandStack& stack,
                  iscore::ObjectLocker& locker,
                  QState* parent):
            CommonState{ObjectPath{scenarioPath}, parent},
            m_dispatcher{std::move(scenarioPath), locker, stack, this}
        {
            using namespace Scenario::Command ;
            auto finalState = new QFinalState{this};
            connect(finalState, &QState::entered, [&] ()
            {
                m_createdEvent = id_type<EventModel>{};
                m_createdTimeNode = id_type<TimeNodeModel>{};
            });

            QState* mainState = new QState{this};
            {
                QState* pressedEventState = new QState{mainState};
                QState* releasedState = new QState{mainState};
                QState* movingEvent = new QState{mainState};

                // General setup
                mainState->setInitialState(pressedEventState);
                releasedState->addTransition(finalState);

                // Pressed -> ...
                auto t_pressed_event = new MoveOnAnything_Transition{*this};
                t_pressed_event->setTargetState(movingEvent);
                pressedEventState->addTransition(t_pressed_event);

                // Moving -> ...
                auto t_moving_event = new MoveOnAnything_Transition{*this};
                t_moving_event->setTargetState(movingEvent);
                movingEvent->addTransition(t_moving_event);

                // Release
                auto t_release = new ReleaseOnAnything_Transition;
                t_release->setTargetState(releasedState);
                mainState->addTransition(t_release);

                // What happens in each state.
                QObject::connect(movingEvent, &QState::entered, [&] ()
                {
                    m_dispatcher.submitCommand(
                                new MoveEvent{
                                    ObjectPath{m_scenarioPath},
                                    clickedEvent,
                                    point.date,
                                    point.y});
                });

                QObject::connect(releasedState, &QState::entered, [&] ()
                {
                    m_dispatcher.commit();
                });
            }

            QState* rollbackState = new QState{this};
            // TODO use event instead.
            // mainState->addTransition(this, SIGNAL(cancel()), rollbackState);
            rollbackState->addTransition(finalState);
            QObject::connect(rollbackState, &QState::entered, [&] ()
            {
                m_dispatcher.rollback();
            });

            setInitialState(mainState);
        }

        LockingOngoingCommandDispatcher<MergeStrategy::Simple, CommitStrategy::Redo> m_dispatcher;
};




class MoveTimeNodeState : public CommonState
{
    public:
        MoveTimeNodeState(ObjectPath&& scenarioPath,
                  iscore::CommandStack& stack,
                  iscore::ObjectLocker& locker,
                  QState* parent):
            CommonState{ObjectPath{scenarioPath}, parent},
            m_dispatcher{std::move(scenarioPath), locker, stack, this}
        {
            using namespace Scenario::Command ;
            auto finalState = new QFinalState{this};
            connect(finalState, &QState::entered, [&] ()
            {
                m_createdEvent = id_type<EventModel>{};
                m_createdTimeNode = id_type<TimeNodeModel>{};
            });

            QState* mainState = new QState{this};
            {
                QState* pressedTimeNodeState = new QState{mainState};
                QState* releasedState = new QState{mainState};
                QState* movingTimeNode = new QState{mainState};
                mainState->setInitialState(pressedTimeNodeState);

                // General setup
                releasedState->addTransition(finalState);

                // Pressed -> ...
                auto t_pressed_timenode = new MoveOnAnything_Transition{*this};
                t_pressed_timenode->setTargetState(movingTimeNode);
                pressedTimeNodeState->addTransition(t_pressed_timenode);

                // Moving -> ...
                auto t_moving_timenode = new MoveOnAnything_Transition{*this};
                t_moving_timenode->setTargetState(movingTimeNode);
                movingTimeNode->addTransition(t_moving_timenode);

                // Release
                auto t_release = new ReleaseOnAnything_Transition;
                t_release->setTargetState(releasedState);
                mainState->addTransition(t_release);

                // What happens in each state.
                QObject::connect(movingTimeNode, &QState::entered, [&] ()
                {
                    /* TODO
            m_dispatcher.submitCommand(
                        new MoveTimeNode{
                            ObjectPath{m_scenarioPath},
                            m_createdEvent,
                            m_scenarioPath.find<ScenarioModel>()->timeNode(hoveredTimeNode)->date(),
                            point.y});
                            */
                });

                QObject::connect(releasedState, &QState::entered, [&] ()
                {
                    m_dispatcher.commit();
                });
            }

            QState* rollbackState = new QState{this};
            // TODO use event instead.
            // mainState->addTransition(this, SIGNAL(cancel()), rollbackState);
            rollbackState->addTransition(finalState);
            QObject::connect(rollbackState, &QState::entered, [&] ()
            {
                m_dispatcher.rollback();
            });

            setInitialState(mainState);
        }

        LockingOngoingCommandDispatcher<MergeStrategy::Simple, CommitStrategy::Redo> m_dispatcher;
};
