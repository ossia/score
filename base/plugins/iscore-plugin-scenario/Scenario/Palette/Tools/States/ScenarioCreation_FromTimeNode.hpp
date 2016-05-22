#pragma once
#include "ScenarioCreationState.hpp"


#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>

#include <Scenario/Commands/Scenario/Displacement/MoveEventMeta.hpp>
#include <Scenario/Commands/Scenario/Displacement/MoveNewEvent.hpp>

#include <Scenario/Commands/Scenario/Creations/CreateConstraint.hpp>

#include <Scenario/Palette/Transitions/NothingTransitions.hpp>
#include <Scenario/Palette/Transitions/AnythingTransitions.hpp>
#include <Scenario/Palette/Transitions/EventTransitions.hpp>
#include <Scenario/Palette/Transitions/StateTransitions.hpp>
#include <Scenario/Palette/Transitions/ConstraintTransitions.hpp>
#include <Scenario/Palette/Transitions/TimeNodeTransitions.hpp>

#include <Scenario/Palette/Tools/ScenarioRollbackStrategy.hpp>
#include <QFinalState>

namespace Scenario
{
template<typename Scenario_T, typename ToolPalette_T>
class Creation_FromTimeNode final : public CreationState<Scenario_T, ToolPalette_T>
{
    public:
        Creation_FromTimeNode(
                const ToolPalette_T& stateMachine,
                const Path<Scenario_T>& scenarioPath,
                const iscore::CommandStackFacade& stack,
                QState* parent):
            CreationState<Scenario_T, ToolPalette_T>{stateMachine, stack, std::move(scenarioPath), parent}
        {
            using namespace Scenario::Command;
            auto finalState = new QFinalState{this};
            QObject::connect(finalState, &QState::entered, [&] ()
            {
                this->clearCreatedIds();
            });

            auto mainState = new QState{this};
            {
                auto pressed = new QState{mainState};
                auto released = new QState{mainState};
                auto move_nothing = new StrongQState<MoveOnNothing>{mainState};
                auto move_state = new StrongQState<MoveOnState>{mainState};
                auto move_event = new StrongQState<MoveOnEvent>{mainState};
                auto move_timenode = new StrongQState<MoveOnTimeNode>{mainState};

                // General setup
                mainState->setInitialState(pressed);
                released->addTransition(finalState);

                // Release
                iscore::make_transition<ReleaseOnAnything_Transition>(mainState, released);

                // Pressed -> ...
                auto t_pressed_moving_nothing =
                        iscore::make_transition<MoveOnNothing_Transition<Scenario_T>>(
                            pressed, move_nothing, *this);

                QObject::connect(t_pressed_moving_nothing, &QAbstractTransition::triggered,
                        [&] ()
                {
                    this->rollback();
                    createToNothing();
                });


                /// MoveOnNothing -> ...
                // MoveOnNothing -> MoveOnNothing.
                iscore::make_transition<MoveOnNothing_Transition<Scenario_T>>(move_nothing, move_nothing, *this);

                // MoveOnNothing -> MoveOnState.
                this->add_transition(move_nothing, move_state,
                               [&] () { this->rollback(); ISCORE_TODO; });

                // MoveOnNothing -> MoveOnEvent.
                this->add_transition(move_nothing, move_event,
                               [&] () {
                    if(this->createdEvents.contains(this->hoveredEvent))
                    {
                        return;
                    }
                    this->rollback();

                    createToEvent();
                });

                // MoveOnNothing -> MoveOnTimeNode
                this->add_transition(move_nothing, move_timenode,
                               [&] () {
                    if(this->createdTimeNodes.contains(this->hoveredTimeNode))
                    {
                        return;
                    }
                    this->rollback();
                    createToTimeNode();
                });


                /// MoveOnState -> ...
                // MoveOnState -> MoveOnNothing
                this->add_transition(move_state, move_nothing,
                               [&] () { this->rollback(); ISCORE_TODO; });

                // MoveOnState -> MoveOnState
                // We don't do anything, the constraint should not move.

                // MoveOnState -> MoveOnEvent
                this->add_transition(move_state, move_event,
                               [&] () { this->rollback(); ISCORE_TODO; });

                // MoveOnState -> MoveOnTimeNode
                this->add_transition(move_state, move_timenode,
                               [&] () { this->rollback(); ISCORE_TODO; });


                /// MoveOnEvent -> ...
                // MoveOnEvent -> MoveOnNothing
                this->add_transition(move_event, move_nothing,
                               [&] () {
                    this->rollback();
                    createToNothing();
                });

                // MoveOnEvent -> MoveOnState
                this->add_transition(move_event, move_state,
                               [&] () { this->rollback(); ISCORE_TODO; });

                // MoveOnEvent -> MoveOnEvent
                iscore::make_transition<MoveOnEvent_Transition<Scenario_T>>(move_event, move_event, *this);

                // MoveOnEvent -> MoveOnTimeNode
                this->add_transition(move_event, move_timenode,
                               [&] () {
                    if(this->createdTimeNodes.contains(this->hoveredTimeNode))
                    {
                        return;
                    }
                    this->rollback();
                    createToTimeNode();
                });


                /// MoveOnTimeNode -> ...
                // MoveOnTimeNode -> MoveOnNothing
                this->add_transition(move_timenode, move_nothing,
                               [&] () {
                    this->rollback();
                    createToNothing();
                });

                // MoveOnTimeNode -> MoveOnState
                this->add_transition(move_timenode, move_state,
                               [&] () { this->rollback(); ISCORE_TODO; });

                // MoveOnTimeNode -> MoveOnEvent
                this->add_transition(move_timenode, move_event,
                               [&] () {
                    if(this->createdEvents.contains(this->hoveredEvent))
                    {
                        this->rollback();
                        return;
                    }
                    this->rollback();
                    createToEvent();
                });

                // MoveOnTimeNode -> MoveOnTimeNode
                iscore::make_transition<MoveOnTimeNode_Transition<Scenario_T>>(move_timenode , move_timenode , *this);



                // What happens in each state.
                QObject::connect(pressed, &QState::entered,
                                 [&] ()
                {
                    this->m_clickedPoint = this->currentPoint;
                    createInitialEventAndState();
                });

                QObject::connect(move_nothing, &QState::entered, [&] ()
                {
                    if(this->createdEvents.empty() || this->createdConstraints.empty())
                    {
                        this->rollback();
                        return;
                    }
                    // Move the timenode
                    this->m_dispatcher.template submitCommand<MoveNewEvent>(
                                Path<Scenario_T>{this->m_scenarioPath},
                                this->createdConstraints.last(),
                                this->createdEvents.last(),
                                this->currentPoint.date,
                                this->currentPoint.y,
                                stateMachine.editionSettings().sequence());
                });

                QObject::connect(move_timenode, &QState::entered, [&] ()
                {
                    if(this->createdEvents.empty())
                    {
                        this->rollback();
                        return;
                    }

                    this->m_dispatcher.template submitCommand<MoveEventMeta>(
                                Path<Scenario_T>{this->m_scenarioPath},
                                this->createdEvents.last(),
                                TimeValue::zero(),
                                0.,
                                stateMachine.editionSettings().expandMode());
                });

                QObject::connect(released, &QState::entered, [&] ()
                {
                    this->makeSnapshot();
                    this->m_dispatcher.template commit<Scenario::Command::CreationMetaCommand>();
                });
            }

            auto rollbackState = new QState{this};
            iscore::make_transition<iscore::Cancel_Transition>(mainState, rollbackState);
            rollbackState->addTransition(finalState);
            QObject::connect(rollbackState, &QState::entered, [&] ()
            {
                this->rollback();
            });

            this->setInitialState(mainState);
        }

    private:
        void createInitialEventAndState()
        {
            auto cmd = new Command::CreateEvent_State{
                    this->m_scenarioPath,
                    this->clickedTimeNode,
                    this->currentPoint.y};
            this->m_dispatcher.submitCommand(cmd);

            this->createdStates.append(cmd->createdState());
            this->createdEvents.append(cmd->createdEvent());
        }

        void createToNothing()
        {
            createInitialEventAndState();
            this->createToNothing_base(this->createdStates.first());
        }
        void createToState()
        {
            createInitialEventAndState();
            this->createToState_base(this->createdStates.first());
        }
        void createToEvent()
        {
            createInitialEventAndState();
            this->createToEvent_base(this->createdStates.first());
        }
        void createToTimeNode()
        {
            // TODO "if hoveredTimeNode != clickedTimeNode"
            createInitialEventAndState();
            this->createToTimeNode_base(this->createdStates.first());
        }
};
}
