#pragma once
#include <Scenario/Commands/MoveBaseEvent.hpp>
#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <Scenario/Palette/Tools/States/MoveStates.hpp>
#include <Scenario/Palette/Tools/SmartTool.hpp>
#include <Scenario/Palette/Transitions/ConstraintTransitions.hpp>
#include <Scenario/Palette/Transitions/EventTransitions.hpp>
#include <Scenario/Palette/Transitions/StateTransitions.hpp>
#include <Scenario/Palette/Transitions/TimeNodeTransitions.hpp>

namespace Scenario
{
class MoveConstraintInBaseScenario_StateWrapper
{
    public:
        template<
                typename Scenario_T,
                typename ToolPalette_T>
        static void make(const ToolPalette_T& palette, QState* waitState, QState& sm)
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
        static void make(const ToolPalette_T& palette, QState* waitState, QState& sm)
        {
            /// Event
            auto moveEvent =
                    new Scenario::MoveEventState<Command::MoveBaseEvent<Scenario_T>, Scenario_T, ToolPalette_T>{
                        palette,
                        palette.model(),
                        palette.context().commandStack,
                        palette.context().objectLocker,
                        &sm};

            iscore::make_transition<Scenario::ClickOnEndState_Transition<Scenario_T>>(waitState,
                                                     moveEvent,
                                                     *moveEvent);

            iscore::make_transition<Scenario::ClickOnEndEvent_Transition<Scenario_T>>(waitState,
                                                     moveEvent,
                                                     *moveEvent);
            moveEvent->addTransition(moveEvent,
                                       finishedState(),
                                       waitState);
        }
};

class MoveTimeNodeInBaseScenario_StateWrapper
{
    public:
        template<
                typename Scenario_T,
                typename ToolPalette_T>
        static void make(const ToolPalette_T& palette, QState* waitState, QState& sm)
        {
            /// TimeNode
            auto moveTimeNode =
                    new Scenario::MoveTimeNodeState<Command::MoveBaseEvent<Scenario_T>, Scenario_T, ToolPalette_T>{
                palette,
                        palette.model(),
                        palette.context().commandStack,
                        palette.context().objectLocker,
                        &sm};

            iscore::make_transition<Scenario::ClickOnEndTimeNode_Transition<Scenario_T>>(waitState,
                                                                    moveTimeNode,
                                                                    *moveTimeNode);
            moveTimeNode->addTransition(moveTimeNode,
                                        finishedState(),
                                        waitState);
        }
};
}
