#pragma once
#include <Scenario/Process/Temporal/StateMachines/ScenarioStateMachineBaseStates.hpp>
#include <Scenario/Process/Temporal/StateMachines/Transitions/AnythingTransitions.hpp>

#include <iscore/command/Dispatchers/SingleOngoingCommandDispatcher.hpp>
#include <iscore/locking/ObjectLocker.hpp>
#include <QFinalState>
namespace Scenario
{

// TODO a nice refactor is doable here between the three classes.
// TODO rename in MoveConstraint_State for hmoegeneity with ClickOnConstraint_Transition,  etc.
template<
        typename MoveConstraintCommand_T, // MoveConstraint
        typename Scenario_T,
        typename ToolPalette_T>
class MoveConstraintState final : public StateBase<Scenario_T>
{
    public:
        MoveConstraintState(const ToolPalette_T& stateMachine,
                            const Path<Scenario_T>& scenarioPath,
                            iscore::CommandStack& stack,
                            iscore::ObjectLocker& locker,
                            QState* parent):
            StateBase<Scenario_T>{scenarioPath, parent},
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
                        iscore::make_transition<MoveOnAnything_Transition<Scenario_T>>(
                            pressed, moving , *this);
                QObject::connect(t_pressed, &QAbstractTransition::triggered, [&] ()
                {
                    auto& cst = this->m_scenarioPath.find().constraint(this->clickedConstraint);
                    m_constraintInitialPoint = {cst.startDate(), cst.heightPercentage()};
                    m_initialClick = this->currentPoint;
                });

                iscore::make_transition<ReleaseOnAnything_Transition>(
                            pressed, finalState);
                iscore::make_transition<MoveOnAnything_Transition<Scenario_T>>(
                            moving , moving , *this);
                iscore::make_transition<ReleaseOnAnything_Transition>(
                            moving , released);

                QObject::connect(moving, &QState::entered, [&] ()
                {
                    this->m_dispatcher.submitCommand(
                                Path<Scenario_T>{this->m_scenarioPath},
                                this->clickedConstraint,
                                m_constraintInitialPoint.date + (this->currentPoint.date - m_initialClick.date),
                                m_constraintInitialPoint.y + (this->currentPoint.y - m_initialClick.y));
                });

                QObject::connect(released, &QState::entered, [&] ()
                {
                    m_dispatcher.commit();
                });
            }

            QState* rollbackState = new QState{this};
            iscore::make_transition<iscore::Cancel_Transition>(mainState, rollbackState);
            rollbackState->addTransition(finalState);
            QObject::connect(rollbackState, &QState::entered, [&] ()
            {
                m_dispatcher.rollback();
            });

            this->setInitialState(mainState);
        }

    SingleOngoingCommandDispatcher<MoveConstraintCommand_T> m_dispatcher;

    private:
        Scenario::Point m_initialClick{};
        Scenario::Point m_constraintInitialPoint{};
};

template<
        typename MoveEventCommand_T, // MoveEventMeta
        typename Scenario_T,
        typename ToolPalette_T>
class MoveEventState final : public StateBase<Scenario_T>
{
    public:
        MoveEventState(const ToolPalette_T& stateMachine,
                       const Path<Scenario_T>& scenarioPath,
                       iscore::CommandStack& stack,
                       iscore::ObjectLocker& locker,
                       QState* parent):
            StateBase<Scenario_T>{scenarioPath, parent},
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

                iscore::make_transition<MoveOnAnything_Transition<Scenario_T>>(
                            pressed, moving, *this);
                iscore::make_transition<ReleaseOnAnything_Transition>(
                            pressed, finalState);
                iscore::make_transition<MoveOnAnything_Transition<Scenario_T>>(
                            moving, moving, *this);
                iscore::make_transition<ReleaseOnAnything_Transition>(
                            moving, released);

                // What happens in each state.
                QObject::connect(moving, &QState::entered, [&] ()
                {
                    Id<EventModel> evId{this->clickedEvent};
                    if(!bool(evId) && bool(this->clickedState))
                    {
                        auto& scenar = this->m_scenarioPath.find();
                        evId = scenar.state(this->clickedState).eventId();
                    }

                    this->m_dispatcher.submitCommand(
                                Path<Scenario_T>{this->m_scenarioPath},
                                evId,
                                this->currentPoint.date,
                                stateMachine.editionSettings().expandMode());
                });

                QObject::connect(released, &QState::entered, [&] ()
                {
                    m_dispatcher.commit();
                });
            }

            QState* rollbackState = new QState{this};
            iscore::make_transition<iscore::Cancel_Transition>(mainState, rollbackState);
            rollbackState->addTransition(finalState);
            QObject::connect(rollbackState, &QState::entered, [&] ()
            {
                m_dispatcher.rollback();
            });

            this->setInitialState(mainState);
        }

        SingleOngoingCommandDispatcher<MoveEventCommand_T> m_dispatcher;
};

template<
        typename MoveTimeNodeCommand_T, // MoveEventMeta
        typename Scenario_T,
        typename ToolPalette_T>
class MoveTimeNodeState final : public StateBase<Scenario_T>
{
    public:
        MoveTimeNodeState(const ToolPalette_T& stateMachine,
                          const Path<Scenario_T>& scenarioPath,
                          iscore::CommandStack& stack,
                          iscore::ObjectLocker& locker,
                          QState* parent):
            StateBase<Scenario_T>{scenarioPath, parent},
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

                iscore::make_transition<MoveOnAnything_Transition<Scenario_T>>(
                            pressed, moving, *this);
                iscore::make_transition<ReleaseOnAnything_Transition>(
                            pressed, finalState);
                iscore::make_transition<MoveOnAnything_Transition<Scenario_T>>(
                            moving, moving, *this);
                iscore::make_transition<ReleaseOnAnything_Transition>(
                            moving, released);

                // What happens in each state.
                QObject::connect(moving, &QState::entered, [&] ()
                {
                    // Get the 1st event on the timenode.
                    auto& scenar = this->m_scenarioPath.find();
                    auto& tn = scenar.timeNode(this->clickedTimeNode);
                    const auto& ev_id = tn.events().first();
                    auto date = this->currentPoint.date;

                    if (!stateMachine.editionSettings().sequence())
                        date = tn.date();

                    m_dispatcher.submitCommand(
                                Path<Scenario_T>{this->m_scenarioPath},
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
            iscore::make_transition<iscore::Cancel_Transition>(mainState, rollbackState);
            rollbackState->addTransition(finalState);
            QObject::connect(rollbackState, &QState::entered, [&] ()
            {
                m_dispatcher.rollback();
            });

            this->setInitialState(mainState);
        }

        SingleOngoingCommandDispatcher<MoveTimeNodeCommand_T> m_dispatcher;
};

}
