#pragma once
#include <Scenario/Commands/Scenario/Displacement/MoveConstraint.hpp>
#include <Scenario/Commands/Scenario/Displacement/MoveEventMeta.hpp>
#include <Scenario/Process/Algorithms/StandardDisplacementPolicy.hpp>

#include <Scenario/Process/Temporal/StateMachines/ScenarioStateMachineBaseStates.hpp>
#include <Scenario/Process/Temporal/StateMachines/Transitions/AnythingTransitions.hpp>

#include <iscore/command/Dispatchers/SingleOngoingCommandDispatcher.hpp>
#include <iscore/locking/ObjectLocker.hpp>
#include <QFinalState>
namespace Scenario
{

// TODO a nice refactor is doable here between the three classes.
// TODO rename in MoveConstraint_State for hmoegeneity with ClickOnConstraint_Transition,  etc.
template<typename Scenario_T, typename ToolPalette_T>
class MoveConstraintState final : public StateBase
{
    public:
        MoveConstraintState(const ToolPalette_T& stateMachine,
                            const Path<Scenario_T>& scenarioPath,
                            iscore::CommandStack& stack,
                            iscore::ObjectLocker& locker,
                            QState* parent):
            StateBase{scenarioPath, parent},
            m_dispatcher{stack}
        {
            this->setObjectName("MoveConstraintState");
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
                    auto& scenar = m_scenarioPath.find();
                    m_constraintInitialStartDate= scenar.constraints.at(clickedConstraint).startDate();
                    m_constraintInitialClickDate = currentPoint.date;
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
                                Path<Scenario_T>{m_scenarioPath},
                                clickedConstraint,
                                m_constraintInitialStartDate + (currentPoint.date - m_constraintInitialClickDate),
                                currentPoint.y);
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

    SingleOngoingCommandDispatcher<Scenario::Command::MoveConstraint> m_dispatcher;

    private:
        TimeValue m_constraintInitialClickDate;
        TimeValue m_constraintInitialStartDate;
};

template<typename Scenario_T, typename ToolPalette_T>
class MoveEventState final : public StateBase
{
    public:
        MoveEventState(const ToolPalette_T& stateMachine,
                       const Path<Scenario_T>& scenarioPath,
                       iscore::CommandStack& stack,
                       iscore::ObjectLocker& locker,
                       QState* parent):
            StateBase{scenarioPath, parent},
            m_dispatcher{stack}
        {
            this->setObjectName("MoveEventState");
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
                    Id<EventModel> evId{clickedEvent};
                    if(!bool(evId) && bool(clickedState))
                    {
                        auto& scenar = m_scenarioPath.find();
                        evId = scenar.state(clickedState).eventId();
                    }

                    m_dispatcher.submitCommand(
                                Path<Scenario_T>{m_scenarioPath},
                                evId,
                                currentPoint.date,
                                stateMachine.editionSettings().expandMode());
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

        SingleOngoingCommandDispatcher<MoveEventMeta> m_dispatcher;
};

template<typename Scenario_T, typename ToolPalette_T>
class MoveTimeNodeState final : public StateBase
{
    public:
        MoveTimeNodeState(const ToolPalette_T& stateMachine,
                          const Path<Scenario_T>& scenarioPath,
                          iscore::CommandStack& stack,
                          iscore::ObjectLocker& locker,
                          QState* parent):
            StateBase{scenarioPath, parent},
            m_dispatcher{stack}
        {
            this->setObjectName("MoveTimeNodeState");
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
                    auto& scenar = m_scenarioPath.find();
                    auto& tn = scenar.timeNodes.at(clickedTimeNode);
                    const auto& ev_id = tn.events().first();
                    auto date = currentPoint.date;

                    if (!stateMachine.editionSettings().sequence())
                        date = tn.date();

                    m_dispatcher.submitCommand(
                                Path<Scenario_T>{m_scenarioPath},
                                ev_id,
                                date,
                                stateMachine.editionSettings().expandMode());
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

        SingleOngoingCommandDispatcher<MoveEventMeta> m_dispatcher;
};

}
