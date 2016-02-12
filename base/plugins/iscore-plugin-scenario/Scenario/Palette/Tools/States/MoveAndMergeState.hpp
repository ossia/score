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

}
