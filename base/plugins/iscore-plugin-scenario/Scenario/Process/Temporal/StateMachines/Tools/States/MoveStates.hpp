#pragma once
#include <iscore/command/Dispatchers/SingleOngoingCommandDispatcher.hpp>
#include <iscore/locking/ObjectLocker.hpp>
#include <Scenario/Process/Temporal/StateMachines/ScenarioStateMachineBaseStates.hpp>
#include <Scenario/Commands/Scenario/Displacement/MoveConstraint.hpp>
#include <Scenario/Commands/Scenario/Displacement/MoveEventMeta.hpp>
#include <Scenario/Process/Algorithms/StandardDisplacementPolicy.hpp>

namespace Scenario
{
class ToolPalette;
// TODO rename in MoveConstraint_State for hmoegeneity with ClickOnConstraint_Transition,  etc.
class MoveConstraintState final : public StateBase
{
    public:
        MoveConstraintState(const Scenario::ToolPalette& stateMachine,
                            const Path<ScenarioModel>& scenarioPath,
                            iscore::CommandStack& stack,
                            iscore::ObjectLocker& locker,
                            QState* parent);

    SingleOngoingCommandDispatcher<Scenario::Command::MoveConstraint> m_dispatcher;

    private:
        TimeValue m_constraintInitialClickDate;
        TimeValue m_constraintInitialStartDate;
};

class MoveEventState final : public StateBase
{
    public:
        MoveEventState(const Scenario::ToolPalette& stateMachine,
                       const Path<ScenarioModel>& scenarioPath,
                       iscore::CommandStack& stack,
                       iscore::ObjectLocker& locker,
                       QState* parent);

        SingleOngoingCommandDispatcher<MoveEventMeta> m_dispatcher;
};

class MoveTimeNodeState final : public StateBase
{
    public:
        MoveTimeNodeState(const Scenario::ToolPalette& stateMachine,
                          const Path<ScenarioModel>& scenarioPath,
                          iscore::CommandStack& stack,
                          iscore::ObjectLocker& locker,
                          QState* parent);

        SingleOngoingCommandDispatcher<MoveEventMeta> m_dispatcher;
};

}
