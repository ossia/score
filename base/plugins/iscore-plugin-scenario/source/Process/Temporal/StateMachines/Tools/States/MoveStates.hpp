#pragma once
#include <iscore/command/OngoingCommandManager.hpp>
#include "Process/Temporal/StateMachines/ScenarioStateMachineBaseStates.hpp"
class ScenarioStateMachine;

class MoveConstraintState : public ScenarioStateBase
{
    public:
        MoveConstraintState(const ScenarioStateMachine& stateMachine,
                            ObjectPath&& scenarioPath,
                            iscore::CommandStack& stack,
                            iscore::ObjectLocker& locker,
                            QState* parent);

    SingleOngoingCommandDispatcher m_dispatcher;

    private:
        TimeValue m_constraintInitialClickDate;
        TimeValue m_constraintInitialStartDate;
};

class MoveEventState : public ScenarioStateBase
{
    public:
        MoveEventState(const ScenarioStateMachine& stateMachine,
                       ObjectPath&& scenarioPath,
                       iscore::CommandStack& stack,
                       iscore::ObjectLocker& locker,
                       QState* parent);

        SingleOngoingCommandDispatcher m_dispatcher;
};

class MoveTimeNodeState : public ScenarioStateBase
{
    public:
        MoveTimeNodeState(const ScenarioStateMachine& stateMachine,
                          ObjectPath&& scenarioPath,
                          iscore::CommandStack& stack,
                          iscore::ObjectLocker& locker,
                          QState* parent);

        SingleOngoingCommandDispatcher m_dispatcher;
};
