#pragma once
#include <Scenario/Palette/Tools/States/MoveStates.hpp>
#include <Scenario/Palette/Tools/States/MoveAndMergeState.hpp>
#include <Scenario/Commands/Scenario/Displacement/MoveConstraint.hpp>
#include <Scenario/Commands/Scenario/Displacement/MoveEventMeta.hpp>
#include <Scenario/Commands/Constraint/SetMinDuration.hpp>
#include <Scenario/Commands/Constraint/SetMaxDuration.hpp>

#include <Scenario/Palette/Transitions/ConstraintTransitions.hpp>
#include <Scenario/Palette/Transitions/EventTransitions.hpp>
#include <Scenario/Palette/Transitions/StateTransitions.hpp>
#include <Scenario/Palette/Transitions/TimeNodeTransitions.hpp>
namespace Scenario
{
class MoveConstraintInScenario_StateWrapper
{
    public:
        template<
                typename Scenario_T,
                typename ToolPalette_T>
        static void make(const ToolPalette_T& palette, QState* waitState, QState& parent)
        {
            /// Constraint
            /// //TODO remove useless arguments to ctor
            auto moveConstraint =
                    new MoveConstraintState<Scenario::Command::MoveConstraint, Scenario_T, ToolPalette_T>{
                        palette,
                        palette.model(),
                        palette.context().context.commandStack,
                        palette.context().context.objectLocker,
                        &parent};

            iscore::make_transition<ClickOnConstraint_Transition<Scenario_T>>(waitState,
                                                          moveConstraint,
                                                          *moveConstraint);
            moveConstraint->addTransition(moveConstraint,
                                          finishedState(),
                                          waitState);
        }
};

class MoveLeftBraceInScenario_StateWrapper
{
    public:
        template<
                typename Scenario_T,
                typename ToolPalette_T>
        static void make(const ToolPalette_T& palette, QState* waitState, QState& parent)
        {
            auto moveBrace =
                    new MoveConstraintBraceState<Scenario::Command::SetMinDuration, Scenario_T, ToolPalette_T>{
                        palette,
                        palette.model(),
                        palette.context().context.commandStack,
                        palette.context().context.objectLocker,
                        &parent};
            iscore::make_transition<ClickOnLeftBrace_Transition<Scenario_T>>(waitState,
                                      moveBrace,
                                      *moveBrace);
            moveBrace->addTransition(moveBrace,
                                     finishedState(),
                                     waitState);
        }
};

class MoveRightBraceInScenario_StateWrapper
{
    public:
        template<
                typename Scenario_T,
                typename ToolPalette_T>
        static void make(const ToolPalette_T& palette, QState* waitState, QState& parent)
        {
            auto moveBrace =
                    new MoveConstraintBraceState<Scenario::Command::SetMaxDuration, Scenario_T, ToolPalette_T>{
                        palette,
                        palette.model(),
                        palette.context().context.commandStack,
                        palette.context().context.objectLocker,
                        &parent};
            iscore::make_transition<ClickOnRightBrace_Transition<Scenario_T>>(waitState,
                                      moveBrace,
                                      *moveBrace);
            moveBrace->addTransition(moveBrace,
                                     finishedState(),
                                     waitState);
        }
};

class MoveEventInScenario_StateWrapper
{
    public:
        template<
                typename Scenario_T,
                typename ToolPalette_T>
        static void make(const ToolPalette_T& palette, QState* waitState, QState& parent)
        {
            /// Event
            auto moveEvent =
                    new MoveEventState<Scenario::Command::MoveEventMeta, Scenario_T, ToolPalette_T>{
                        palette,
                        palette.model(),
                        palette.context().context.commandStack,
                        palette.context().context.objectLocker,
                        &parent};

            iscore::make_transition<ClickOnState_Transition<Scenario_T>>(waitState,
                                                     moveEvent,
                                                     *moveEvent);

            iscore::make_transition<ClickOnEvent_Transition<Scenario_T>>(waitState,
                                                     moveEvent,
                                                     *moveEvent);
            moveEvent->addTransition(moveEvent,
                                       finishedState(),
                                       waitState);
        }
};

class MoveTimeNodeInScenario_StateWrapper
{
    public:
        template<
                typename Scenario_T,
                typename ToolPalette_T>
        static void make(const ToolPalette_T& palette, QState* waitState, QState& parent)
        {
            /// TimeNode
            auto moveTimeNode =
                    new MoveTimeNodeState<Scenario::Command::MoveEventMeta, Scenario_T, ToolPalette_T>{
                palette,
                        palette.model(),
                        palette.context().context.commandStack,
                        palette.context().context.objectLocker,
                        &parent};

            iscore::make_transition<ClickOnTimeNode_Transition<Scenario_T>>(waitState,
                                                                    moveTimeNode,
                                                                    *moveTimeNode);
            moveTimeNode->addTransition(moveTimeNode,
                                        finishedState(),
                                        waitState);
        }
};
}
