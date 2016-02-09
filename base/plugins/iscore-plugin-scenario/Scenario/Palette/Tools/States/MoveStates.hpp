#pragma once
#include <Scenario/Palette/ScenarioPaletteBaseStates.hpp>
#include <Scenario/Palette/Transitions/AnythingTransitions.hpp>
#include <Scenario/Palette/Transitions/NothingTransitions.hpp>
#include <Scenario/Palette/Transitions/TimeNodeTransitions.hpp>
#include <Scenario/Palette/Transitions/EventTransitions.hpp>
#include <Scenario/Palette/Transitions/ConstraintTransitions.hpp>
#include <Scenario/Palette/Tools/ScenarioRollbackStrategy.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>

#include <Scenario/Commands/Scenario/Merge/MergeTimeNodes.hpp>
#include <Scenario/Commands/Scenario/Merge/MergeEvents.hpp>

#include <iscore/command/Dispatchers/SingleOngoingCommandDispatcher.hpp>
#include <iscore/locking/ObjectLocker.hpp>
#include <QFinalState>
namespace Scenario
{

// TODO a nice refactor is doable here between the four classes.
// TODO rename in MoveConstraint_State for homoegeneity with ClickOnConstraint_Transition,  etc.
template<
        typename MoveConstraintCommand_T, // MoveConstraint
        typename Scenario_T,
        typename ToolPalette_T>
class MoveConstraintState final : public StateBase<Scenario_T>
{
    public:
        MoveConstraintState(const ToolPalette_T& stateMachine,
                            const Path<Scenario_T>& scenarioPath,
                            iscore::CommandStackFacade& stack,
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
        typename MoveBraceCommand_T, // SetMinDuration or setMaxDuration
        typename Scenario_T,
        typename ToolPalette_T>
class MoveConstraintBraceState final : public StateBase<Scenario_T>
{
    public:
        MoveConstraintBraceState(const ToolPalette_T& stateMachine,
                                const Path<Scenario_T>& scenarioPath,
                                iscore::CommandStackFacade& stack,
                                iscore::ObjectLocker& locker,
                                QState* parent):
            StateBase<Scenario_T>{scenarioPath, parent},
            m_dispatcher{stack}
    {
            this->setObjectName("MoveConstraintBraceState");
            using namespace Scenario::Command ;
            auto finalState = new QFinalState{this};

            QState* mainState = new QState{this};
            {
                QState* pressed = new QState{mainState};
                QState* released = new QState{mainState};
                QState* moving = new QState{mainState};

                mainState->setInitialState(pressed);
                released->addTransition(finalState);


                iscore::make_transition<MoveOnAnything_Transition<Scenario_T>>(
                            pressed, moving, *this);
                iscore::make_transition<ReleaseOnAnything_Transition>(
                            pressed, finalState);

                iscore::make_transition<MoveOnAnything_Transition<Scenario_T>>(
                            moving , moving , *this);
                iscore::make_transition<ReleaseOnAnything_Transition>(
                            moving , released);

                QObject::connect(pressed, &QState::entered, [&] ()
                {
                    this->m_initialDate = this->currentPoint.date;
                    auto& scenar = stateMachine.model();
                    auto& cstr = scenar.constraint(this->clickedConstraint);
                    this->m_initialDuration = ((cstr.duration).*MoveBraceCommand_T::corresponding_member)(); // = constraint MinDuration or maxDuration
                });

                QObject::connect(moving, &QState::entered, [&] ()
                {
                    auto& scenar = stateMachine.model();
                    auto& cstr = scenar.constraint(this->clickedConstraint);
                    auto date = this->currentPoint.date - *m_initialDate + *m_initialDuration;
                    this->m_dispatcher.submitCommand(
                                Path<ConstraintModel>{cstr},
                                date,
                                false);
                });

                QObject::connect(released, &QState::entered, [&] ()
                {
                    this->m_dispatcher.commit();
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
        SingleOngoingCommandDispatcher<MoveBraceCommand_T> m_dispatcher;

    private:
        boost::optional<TimeValue> m_initialDate;
        boost::optional<TimeValue> m_initialDuration;

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
                       iscore::CommandStackFacade& stack,
                       iscore::ObjectLocker& locker,
                       QState* parent):
            StateBase<Scenario_T>{scenarioPath, parent},
            m_movingDispatcher{stack},
            m_mergingTnDispatcher{stack},
            m_mergingEventDispatcher{stack}
        {
            this->setObjectName("MoveEventState");
            using namespace Scenario::Command ;
            auto finalState = new QFinalState{this};

            QState* mainState = new QState{this};
            {
                QState* pressed = new QState{mainState};
                QState* released = new QState{mainState};

                QState* onlyMoving = new QState{mainState};
                QState* mergingOnTimeNode = new QState{mainState};
                QState* mergingOnEvent = new QState{mainState};

                QState* rollbackTnMerging = new QState{mainState};
                QState* rollbackEventMerging = new QState{mainState};

                // General setup
                mainState->setInitialState(pressed);
                released->addTransition(finalState);

                rollbackTnMerging->addTransition(onlyMoving);
                rollbackEventMerging->addTransition(onlyMoving);

                // ***************************************
                // transitions

                // press
                iscore::make_transition<MoveOnAnything_Transition<Scenario_T>>(
                            pressed, onlyMoving, *this);
                iscore::make_transition<ReleaseOnAnything_Transition>(
                            pressed, finalState);

                // update commands
                iscore::make_transition<MoveOnAnythingButPonctual_Transition<Scenario_T>>(
                            onlyMoving, onlyMoving, *this);

                iscore::make_transition<MoveOnTimeNode_Transition<Scenario_T>>(
                            onlyMoving, mergingOnTimeNode, *this);

                iscore::make_transition<MoveOnEvent_Transition<Scenario_T>>(
                            onlyMoving, mergingOnEvent, *this);

                // rollback merging
                iscore::make_transition<MoveOnAnythingButTimeNode_Transition<Scenario_T>>(
                            mergingOnTimeNode, rollbackTnMerging, *this);
                iscore::make_transition<MoveOnAnythingButEvent_Transition<Scenario_T>>(
                            mergingOnEvent, rollbackEventMerging, *this);

                // commit merging
                iscore::make_transition<ReleaseOnAnything_Transition>(
                            mergingOnTimeNode, released);
                iscore::make_transition<ReleaseOnAnything_Transition>(
                            mergingOnEvent, released);
                iscore::make_transition<ReleaseOnAnything_Transition>(
                            onlyMoving, released);


                // ********************************************
                // What happens in each state.

                QObject::connect(mergingOnTimeNode, &QState::entered, [&] ()
                {
                    auto& scenar = stateMachine.model();
                    // If we came here through a state.
                    Id<EventModel> evId{this->clickedEvent};
                    if(!bool(evId) && bool(this->clickedState))
                    {
                        evId = scenar.state(this->clickedState).eventId();
                    }
                    Id<TimeNodeModel> tnId = scenar.event(evId).timeNode();

                    if(tnId != this->hoveredTimeNode)
                    {
                        this->m_movingDispatcher.rollback();

                        this->m_mergingTnDispatcher.submitCommand(
                                    Path<Scenario_T>{this->m_scenarioPath},
                                    tnId,
                                    this->hoveredTimeNode);
                    }

                });
                QObject::connect(rollbackTnMerging, &QState::entered, [&] ()
                {
                    m_mergingTnDispatcher.rollback();
                });

                QObject::connect(mergingOnEvent, &QState::entered, [&] ()
                {
                    auto& scenar = stateMachine.model();
                    // If we came here through a state.
                    Id<EventModel> clickedEvId{this->clickedEvent};
                    if(!bool(clickedEvId) && bool(this->clickedState))
                    {
                        clickedEvId = scenar.state(this->clickedState).eventId();
                    }

                    Id<EventModel> destinationEvId{this->hoveredEvent};
                    if(!bool(destinationEvId) && bool(this->hoveredState))
                    {
                        destinationEvId = scenar.state(this->hoveredState).eventId();
                    }

                    if(clickedEvId != destinationEvId)
                    {
                        m_movingDispatcher.rollback();

                        m_mergingEventDispatcher.submitCommand(
                                    Path<Scenario_T>{this->m_scenarioPath},
                                    clickedEvId,
                                    destinationEvId);
                    }
                });
                QObject::connect(rollbackEventMerging, &QState::entered, [&] ()
                {
                    m_mergingEventDispatcher.rollback();
                });


                QObject::connect(onlyMoving, &QState::entered, [&] ()
                {
                    auto& scenar = stateMachine.model();
                    // If we came here through a state.
                    Id<EventModel> evId{this->clickedEvent};
                    if(!bool(evId) && bool(this->clickedState))
                    {
                        evId = scenar.state(this->clickedState).eventId();
                    }

                    TimeValue date = this->m_pressedPrevious
                            ? max(this->currentPoint.date, *this->m_pressedPrevious)
                            : this->currentPoint.date;

                    this->m_movingDispatcher.submitCommand(
                                Path<Scenario_T>{this->m_scenarioPath},
                                evId,
                                date,
                                stateMachine.editionSettings().expandMode() );
                });

                QObject::connect(pressed, &QState::entered, [&] ()
                {
                    auto& scenar = stateMachine.model();
                    Id<EventModel> evId{this->clickedEvent};
                    if(!bool(evId) && bool(this->clickedState))
                    {
                        evId = scenar.state(this->clickedState).eventId();
                    }

                    auto prev_csts = previousConstraints(scenar.event(evId), scenar);
                    if(!prev_csts.empty())
                    {
                        // We find the one that starts the latest.
                        TimeValue t = TimeValue::zero();
                        for(const auto& cst_id : prev_csts)
                        {
                            const auto& other_date = scenar.constraint(cst_id).startDate();
                            if(other_date > t)
                                t = other_date;
                        }
                        this->m_pressedPrevious = t + TimeValue::fromMsecs(10);
                    }
                    else
                    {
                        this->m_pressedPrevious.reset();
                    }

                });


                QObject::connect(released, &QState::entered, [&] ()
                {
                    m_movingDispatcher.commit();
                    m_mergingEventDispatcher.commit();
                    m_mergingTnDispatcher.commit();
                });
            }

            QState* rollbackState = new QState{this};
            iscore::make_transition<iscore::Cancel_Transition>(mainState, rollbackState);
            rollbackState->addTransition(finalState);
            QObject::connect(rollbackState, &QState::entered, [&] ()
            {
                m_movingDispatcher.rollback();
                m_mergingTnDispatcher.rollback();
                m_mergingEventDispatcher.rollback();
            });

            this->setInitialState(mainState);
        }

        SingleOngoingCommandDispatcher<MoveEventCommand_T> m_movingDispatcher;
        SingleOngoingCommandDispatcher<Command::MergeTimeNodes<Scenario_T>> m_mergingTnDispatcher;
        SingleOngoingCommandDispatcher<Command::MergeEvents<Scenario_T>> m_mergingEventDispatcher;

        boost::optional<TimeValue> m_pressedPrevious;
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
                          iscore::CommandStackFacade& stack,
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
                QObject::connect(pressed, &QState::entered, [&] ()
                {
                    auto& scenar = stateMachine.model();

                    auto prev_csts = previousConstraints(
                                scenar.timeNode(this->clickedTimeNode),
                                scenar);
                    if(!prev_csts.empty())
                    {
                        // We find the one that starts the latest.
                        TimeValue t = TimeValue::zero();
                        for(const auto& cst_id : prev_csts)
                        {
                            const auto& other_date = scenar.constraint(cst_id).startDate();
                            if(other_date > t)
                                t = other_date;
                        }
                        this->m_pressedPrevious = t;
                    }
                    else
                    {
                        this->m_pressedPrevious.reset();
                    }

                });


                QObject::connect(moving, &QState::entered, [&] ()
                {
                    // Get the 1st event on the timenode.
                    auto& scenar = stateMachine.model();
                    auto& tn = scenar.timeNode(this->clickedTimeNode);
                    const auto& ev_id = tn.events().first();
                    auto date = this->currentPoint.date;

                    if (!stateMachine.editionSettings().sequence())
                    {
                        // TODO why??
                        date = tn.date();
                    }

                    if(this->m_pressedPrevious)
                    {
                        date = max(date, *this->m_pressedPrevious);
                    }

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
        boost::optional<TimeValue> m_pressedPrevious;
};

}
