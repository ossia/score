#pragma once
#include <iscore/command/OngoingCommandManager.hpp>
#include "Process/Temporal/StateMachines/StateMachineCommon.hpp"

class MoveConstraintState : public CommonScenarioState
{
    public:
        MoveConstraintState(ObjectPath&& scenarioPath,
                  iscore::CommandStack& stack,
                  iscore::ObjectLocker& locker,
                  QState* parent);

        LockingOngoingCommandDispatcher<MergeStrategy::Simple, CommitStrategy::Redo> m_dispatcher;

    private:
        TimeValue m_constraintInitialClickDate;
        TimeValue m_constraintInitialStartDate;
};

class MoveEventState : public CommonScenarioState
{
    public:
        MoveEventState(ObjectPath&& scenarioPath,
                  iscore::CommandStack& stack,
                  iscore::ObjectLocker& locker,
                  QState* parent);

        LockingOngoingCommandDispatcher<MergeStrategy::Simple, CommitStrategy::Redo> m_dispatcher;
};

class MoveTimeNodeState : public CommonScenarioState
{
    public:
        MoveTimeNodeState(ObjectPath&& scenarioPath,
                  iscore::CommandStack& stack,
                  iscore::ObjectLocker& locker,
                  QState* parent);

        LockingOngoingCommandDispatcher<MergeStrategy::Simple, CommitStrategy::Redo> m_dispatcher;
};
