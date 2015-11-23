#pragma once
#include <Scenario/Process/Temporal/StateMachines/Tools/States/MoveStates.hpp>
#include <Scenario/Commands/Scenario/Displacement/MoveConstraint.hpp>
#include <Scenario/Commands/Scenario/Displacement/MoveEventMeta.hpp>

#include <Scenario/Process/Temporal/StateMachines/Transitions/ConstraintTransitions.hpp>
#include <Scenario/Process/Temporal/StateMachines/Transitions/EventTransitions.hpp>
#include <Scenario/Process/Temporal/StateMachines/Transitions/StateTransitions.hpp>
#include <Scenario/Process/Temporal/StateMachines/Transitions/TimeNodeTransitions.hpp>
namespace Scenario
{
class MoveConstraintInScenario_StateWrapper
{
    public:
        template<
                typename Scenario_T,
                typename ToolPalette_T>
        static void make(const ToolPalette_T& palette, QState* waitState, QStateMachine& sm)
        {
            /// Constraint
            /// //TODO remove useless arguments to ctor
            auto moveConstraint =
                    new MoveConstraintState<MoveConstraint, Scenario_T, ToolPalette_T>{
                        palette,
                        palette.model(),
                        palette.context().commandStack,
                        palette.context().objectLocker,
                        nullptr};

            make_transition<ClickOnConstraint_Transition<Scenario_T>>(waitState,
                                                          moveConstraint,
                                                          *moveConstraint);
            moveConstraint->addTransition(moveConstraint,
                                          SIGNAL(finished()),
                                          waitState);
            sm.addState(moveConstraint);
        }
};

class MoveEventInScenario_StateWrapper
{
    public:
        template<
                typename Scenario_T,
                typename ToolPalette_T>
        static void make(const ToolPalette_T& palette, QState* waitState, QStateMachine& sm)
        {
            /// Event
            auto moveEvent =
                    new MoveEventState<MoveEventMeta, Scenario_T, ToolPalette_T>{
                        palette,
                        palette.model(),
                        palette.context().commandStack,
                        palette.context().objectLocker,
                        nullptr};

            make_transition<ClickOnState_Transition<Scenario_T>>(waitState,
                                                     moveEvent,
                                                     *moveEvent);

            make_transition<ClickOnEvent_Transition<Scenario_T>>(waitState,
                                                     moveEvent,
                                                     *moveEvent);
            moveEvent->addTransition(moveEvent,
                                       SIGNAL(finished()),
                                       waitState);
            sm.addState(moveEvent);
        }
};

class MoveTimeNodeInScenario_StateWrapper
{
    public:
        template<
                typename Scenario_T,
                typename ToolPalette_T>
        static void make(const ToolPalette_T& palette, QState* waitState, QStateMachine& sm)
        {
            /// TimeNode
            auto moveTimeNode =
                    new MoveTimeNodeState<MoveEventMeta, Scenario_T, ToolPalette_T>{
                palette,
                        palette.model(),
                        palette.context().commandStack,
                        palette.context().objectLocker,
                        nullptr};

            make_transition<ClickOnTimeNode_Transition<Scenario_T>>(waitState,
                                                                    moveTimeNode,
                                                                    *moveTimeNode);
            moveTimeNode->addTransition(moveTimeNode,
                                        SIGNAL(finished()),
                                        waitState);
            sm.addState(moveTimeNode);
        }
};
}
