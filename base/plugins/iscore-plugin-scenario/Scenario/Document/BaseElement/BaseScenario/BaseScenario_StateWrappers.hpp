#pragma once
#include <Scenario/Commands/ResizeBaseConstraint.hpp>
#include <Scenario/Document/BaseElement/BaseScenario/BaseScenario.hpp>
#include <Scenario/Process/Temporal/StateMachines/Tools/States/MoveStates.hpp>
#include <Scenario/Process/Temporal/StateMachines/Tools/SelectionToolState.hpp>
#include <Scenario/Process/Temporal/StateMachines/Transitions/ConstraintTransitions.hpp>
#include <Scenario/Process/Temporal/StateMachines/Transitions/EventTransitions.hpp>
#include <Scenario/Process/Temporal/StateMachines/Transitions/StateTransitions.hpp>
#include <Scenario/Process/Temporal/StateMachines/Transitions/TimeNodeTransitions.hpp>


class MoveConstraintInBaseScenario_StateWrapper
{
    public:
        template<
                typename Scenario_T,
                typename ToolPalette_T>
        static void make(const ToolPalette_T& palette, QState* waitState, QStateMachine& sm)
        {
            // We cannot move the constraint
        }
};

class MoveEventInBaseScenario_StateWrapper
{
    public:
        template<
                typename Scenario_T,
                typename ToolPalette_T>
        static void make(const ToolPalette_T& palette, QState* waitState, QStateMachine& sm)
        {
            /// Event
            auto moveEvent =
                    new Scenario::MoveEventState<MoveBaseEvent<Scenario_T>, Scenario_T, ToolPalette_T>{
                        palette,
                        palette.model(),
                        palette.context().commandStack,
                        palette.context().objectLocker,
                        nullptr};

            make_transition<Scenario::ClickOnState_Transition<Scenario_T>>(waitState,
                                                     moveEvent,
                                                     *moveEvent);

            make_transition<Scenario::ClickOnEvent_Transition<Scenario_T>>(waitState,
                                                     moveEvent,
                                                     *moveEvent);
            moveEvent->addTransition(moveEvent,
                                       SIGNAL(finished()),
                                       waitState);
            sm.addState(moveEvent);
        }
};

class MoveTimeNodeInBaseScenario_StateWrapper
{
    public:
        template<
                typename Scenario_T,
                typename ToolPalette_T>
        static void make(const ToolPalette_T& palette, QState* waitState, QStateMachine& sm)
        {
            /// TimeNode
            auto moveTimeNode =
                    new Scenario::MoveTimeNodeState<MoveBaseEvent<Scenario_T>, Scenario_T, ToolPalette_T>{
                palette,
                        palette.model(),
                        palette.context().commandStack,
                        palette.context().objectLocker,
                        nullptr};

            make_transition<Scenario::ClickOnTimeNode_Transition<Scenario_T>>(waitState,
                                                                    moveTimeNode,
                                                                    *moveTimeNode);
            moveTimeNode->addTransition(moveTimeNode,
                                        SIGNAL(finished()),
                                        waitState);
            sm.addState(moveTimeNode);
        }
};
