#pragma once
#include <iscore/command/Dispatchers/SingleOngoingCommandDispatcher.hpp>
#include <iscore/locking/ObjectLocker.hpp>
#include "Process/Temporal/StateMachines/ScenarioStateMachineBaseStates.hpp"
#include "Commands/Scenario/Displacement/MoveConstraint.hpp"
#include "Commands/Scenario/Displacement/MoveEvent.hpp"
class ScenarioStateMachine;

class MoveConstraintState : public ScenarioStateBase
{
    public:
        MoveConstraintState(const ScenarioStateMachine& stateMachine,
                            const Path<ScenarioModel>& scenarioPath,
                            iscore::CommandStack& stack,
                            iscore::ObjectLocker& locker,
                            QState* parent);

    SingleOngoingCommandDispatcher<Scenario::Command::MoveConstraint> m_dispatcher;

    private:
        TimeValue m_constraintInitialClickDate;
        TimeValue m_constraintInitialStartDate;
};

class MoveEventState : public ScenarioStateBase
{
    public:
        MoveEventState(const ScenarioStateMachine& stateMachine,
                       const Path<ScenarioModel>& scenarioPath,
                       iscore::CommandStack& stack,
                       iscore::ObjectLocker& locker,
                       QState* parent);

        SingleOngoingCommandDispatcher<Scenario::Command::MoveEvent> m_dispatcher;
};

class MoveTimeNodeState : public ScenarioStateBase
{
    public:
        MoveTimeNodeState(const ScenarioStateMachine& stateMachine,
                          const Path<ScenarioModel>& scenarioPath,
                          iscore::CommandStack& stack,
                          iscore::ObjectLocker& locker,
                          QState* parent);

        SingleOngoingCommandDispatcher<Scenario::Command::MoveEvent> m_dispatcher;
};
