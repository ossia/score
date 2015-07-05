#pragma once
#include <iscore/command/Dispatchers/MultiOngoingCommandDispatcher.hpp>
#include "Process/Temporal/StateMachines/ScenarioStateMachineBaseStates.hpp"
#include "Commands/Scenario/Creations/CreationMetaCommand.hpp"
#include "../ScenarioRollbackStrategy.hpp"
#include "Process/Temporal/StateMachines/ScenarioStateMachineBaseTransitions.hpp"

template<int Value>
class StrongQState : public QState
{
    public:
        static constexpr auto value() { return Value; }
        using QState::QState;
};
class ScenarioStateMachine;

// Here to prevent pollution of the CreationState header with the command dispatcher
class ScenarioCreationState : public CreationState
{
    public:
        ScenarioCreationState(
                iscore::CommandStack& stack,
                ObjectPath&& scenarioPath,
                QState* parent):
            CreationState{std::forward<ObjectPath>(scenarioPath), parent},
            m_dispatcher{stack}
        {

        }

    protected:
        template<typename OriginState, typename DestinationState, typename Function>
        void makeTransition(OriginState* from, DestinationState* to, Function&& fun)
        {
            using transition_type = ScenarioTransition_T<DestinationState::value()>;
            auto trans = make_transition<transition_type>(from, to, *this);
            trans->setObjectName(QString::number(DestinationState::value()));
            connect(trans,
                    &transition_type::triggered,
                    this, fun);
        }

        void rollback() { m_dispatcher.rollback<ScenarioRollbackStrategy>(); clearCreatedIds(); }
        MultiOngoingCommandDispatcher m_dispatcher;

        ScenarioPoint m_clickedPoint;
};



