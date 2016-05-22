#pragma once

#include <Scenario/Commands/Scenario/Merge/MergeTimeNodes.hpp>
#include <Scenario/Commands/Scenario/Merge/MergeEvents.hpp>

#include <Scenario/Palette/ScenarioPaletteBaseStates.hpp>
#include <Scenario/Palette/Transitions/AnythingTransitions.hpp>
#include <Scenario/Palette/Transitions/NothingTransitions.hpp>
#include <Scenario/Palette/Transitions/TimeNodeTransitions.hpp>
#include <Scenario/Palette/Transitions/EventTransitions.hpp>


#include <iscore/command/Dispatchers/SingleOngoingCommandDispatcher.hpp>

#include <QFinalState>


namespace Scenario
{

template<
    typename MoveEventCommand_T, // MoveEventMeta
    typename Scenario_T,
    typename ToolPalette_T>
class MoveEventState final : public StateBase<Scenario_T>
{
    public:
    MoveEventState(const ToolPalette_T& stateMachine,
               const Path<Scenario_T>& scenarioPath,
               const iscore::CommandStackFacade& stack,
               iscore::ObjectLocker& locker,
               QState* parent):
        StateBase<Scenario_T>{scenarioPath, parent},
        m_movingDispatcher{stack}
    {
        this->setObjectName("MoveEventState");
        using namespace Scenario::Command ;
        auto finalState = new QFinalState{this};

        auto mainState = new QState{this};
        {
            auto pressed = new QState{mainState};
            auto released = new QState{mainState};

            auto onlyMoving = new QState{mainState};

            // General setup
            mainState->setInitialState(pressed);
            released->addTransition(finalState);


            // ***************************************
            // transitions

            // press
            iscore::make_transition<MoveOnAnything_Transition<Scenario_T>>(
                    pressed, onlyMoving, *this);
            iscore::make_transition<ReleaseOnAnything_Transition>(
                    pressed, finalState);

            // update commands
            iscore::make_transition<MoveOnAnything_Transition<Scenario_T>>(
                    onlyMoving, onlyMoving, *this);

            // commit merging
            iscore::make_transition<ReleaseOnAnything_Transition>(
                    onlyMoving, released);


            // ********************************************
            // What happens in each state.

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
                    this->currentPoint.y,
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

                    // These 10 milliseconds are here to prevent "squashing"
                    // processes to zero, which leads to problem (they can't scale back!)
                    this->m_pressedPrevious = t + TimeValue::fromMsecs(10);
                }
                else
                {
                    this->m_pressedPrevious = iscore::none;
                }

            });


            QObject::connect(released, &QState::entered, [&] ()
            {
                m_movingDispatcher.commit();
            });
        }

        auto rollbackState = new QState{this};
        iscore::make_transition<iscore::Cancel_Transition>(mainState, rollbackState);
        rollbackState->addTransition(finalState);
        QObject::connect(rollbackState, &QState::entered, [&] ()
        {
            m_movingDispatcher.rollback();
        });

        this->setInitialState(mainState);
    }

    SingleOngoingCommandDispatcher<MoveEventCommand_T> m_movingDispatcher;
    optional<TimeValue> m_pressedPrevious;
};


/**
 * Specialization for the ScenarioModel allows merging
 */
template<
    typename MoveEventCommand_T, // MoveEventMeta
    typename ToolPalette_T>
class MoveEventState<MoveEventCommand_T, Scenario::ScenarioModel, ToolPalette_T> final :
        public StateBase<Scenario::ScenarioModel>
{
    public:
    MoveEventState(const ToolPalette_T& stateMachine,
               const Path<Scenario::ScenarioModel>& scenarioPath,
               const iscore::CommandStackFacade& stack,
               iscore::ObjectLocker& locker,
               QState* parent):
        StateBase<Scenario::ScenarioModel>{scenarioPath, parent},
        m_movingDispatcher{stack},
        m_mergingTnDispatcher{stack},
        m_mergingEventDispatcher{stack}
    {
        this->setObjectName("MoveEventState");
        using namespace Scenario::Command ;
        auto finalState = new QFinalState{this};

        auto mainState = new QState{this};
        {
            auto pressed = new QState{mainState};
            auto released = new QState{mainState};

            auto onlyMoving = new QState{mainState};
            auto mergingOnTimeNode = new QState{mainState};
            auto mergingOnEvent = new QState{mainState};

            auto rollbackTnMerging = new QState{mainState};
            auto rollbackEventMerging = new QState{mainState};

            // General setup
            mainState->setInitialState(pressed);
            released->addTransition(finalState);

            rollbackTnMerging->addTransition(onlyMoving);
            rollbackEventMerging->addTransition(onlyMoving);

            // ***************************************
            // transitions

            // press
            iscore::make_transition<MoveOnAnything_Transition<Scenario::ScenarioModel>>(
                    pressed, onlyMoving, *this);
            iscore::make_transition<ReleaseOnAnything_Transition>(
                    pressed, finalState);

            // update commands
//            iscore::make_transition<MoveOnAnything_Transition<Scenario_T>>(
//                    onlyMoving, onlyMoving, *this);
//*
            iscore::make_transition<MoveOnAnythingButPonctual_Transition<Scenario::ScenarioModel>>(
                    onlyMoving, onlyMoving, *this);

            iscore::make_transition<MoveOnTimeNode_Transition<Scenario::ScenarioModel>>(
                    onlyMoving, mergingOnTimeNode, *this);

            iscore::make_transition<MoveOnEvent_Transition<Scenario::ScenarioModel>>(
                    onlyMoving, mergingOnEvent, *this);
//*/
            // rollback merging
            iscore::make_transition<MoveOnAnythingButTimeNode_Transition<Scenario::ScenarioModel>>(
                    mergingOnTimeNode, rollbackTnMerging, *this);
            iscore::make_transition<MoveOnAnythingButEvent_Transition<Scenario::ScenarioModel>>(
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
                    this->m_mergingEventDispatcher.rollback();

                    this->m_mergingTnDispatcher.submitCommand(
                            Path<Scenario::ScenarioModel>{this->m_scenarioPath},
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
                    m_mergingTnDispatcher.rollback();

                    m_mergingEventDispatcher.submitCommand(
                            Path<Scenario::ScenarioModel>{this->m_scenarioPath},
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
                    Path<Scenario::ScenarioModel>{this->m_scenarioPath},
                    evId,
                    date,
                    this->currentPoint.y,
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

                    // These 10 milliseconds are here to prevent "squashing"
                    // processes to zero, which leads to problem (they can't scale back!)
                    this->m_pressedPrevious = t + TimeValue::fromMsecs(10);
                }
                else
                {
                    this->m_pressedPrevious = iscore::none;
                }

            });


            QObject::connect(released, &QState::entered, [&] ()
            {
                m_movingDispatcher.commit();
                m_mergingEventDispatcher.commit();
                m_mergingTnDispatcher.commit();
            });
        }

        auto rollbackState = new QState{this};
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
    SingleOngoingCommandDispatcher<Command::MergeTimeNodes<Scenario::ScenarioModel>> m_mergingTnDispatcher;
    SingleOngoingCommandDispatcher<Command::MergeEvents<Scenario::ScenarioModel>> m_mergingEventDispatcher;

    optional<TimeValue> m_pressedPrevious;
};

}
