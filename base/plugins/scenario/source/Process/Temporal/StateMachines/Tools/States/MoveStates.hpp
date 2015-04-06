#pragma once
#include <iscore/command/OngoingCommandManager.hpp>
#include "Process/Temporal/StateMachines/StateMachineCommon.hpp"

class MoveConstraintState : public CommonState
{
    public:
        MoveConstraintState(ObjectPath&& scenarioPath,
                  iscore::CommandStack& stack,
                  iscore::ObjectLocker& locker,
                  QState* parent);

        LockingOngoingCommandDispatcher<MergeStrategy::Simple, CommitStrategy::Redo> m_dispatcher;
};

class MoveEventState : public CommonState
{
    public:
        MoveEventState(ObjectPath&& scenarioPath,
                  iscore::CommandStack& stack,
                  iscore::ObjectLocker& locker,
                  QState* parent);

        LockingOngoingCommandDispatcher<MergeStrategy::Simple, CommitStrategy::Redo> m_dispatcher;
};

class MoveTimeNodeState : public CommonState
{
    public:
        MoveTimeNodeState(ObjectPath&& scenarioPath,
                  iscore::CommandStack& stack,
                  iscore::ObjectLocker& locker,
                  QState* parent);

        LockingOngoingCommandDispatcher<MergeStrategy::Simple, CommitStrategy::Redo> m_dispatcher;
};
