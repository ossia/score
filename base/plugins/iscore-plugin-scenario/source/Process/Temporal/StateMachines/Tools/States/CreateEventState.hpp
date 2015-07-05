#pragma once
#include <iscore/command/Dispatchers/MultiOngoingCommandDispatcher.hpp>
#include "Process/Temporal/StateMachines/ScenarioStateMachineBaseStates.hpp"
#include "Commands/Scenario/Creations/CreationMetaCommand.hpp"
#include "Process/Temporal/StateMachines/ScenarioStateMachineBaseTransitions.hpp"

#include "../ScenarioRollbackStrategy.hpp"
template<int Value>
class StrongQState : public QState
{
    public:
        static constexpr auto value() { return Value; }
        using QState::QState;
};
class ScenarioStateMachine;

// TODO correctly separate in files
class CreateFromEventState : public CreationState
{
    public:
        CreateFromEventState(
                const ScenarioStateMachine& stateMachine,
                ObjectPath&& scenarioPath,
                iscore::CommandStack& stack,
                QState* parent);

    private:
        void rollback() { m_dispatcher.rollback<ScenarioRollbackStrategy>(); }

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


        void createEventFromEventOnNothing();
        void createConstraintBetweenEventAndTimeNode();

        void createConstraintBetweenEventAndState();
        void createConstraintBetweenEvents();
        void createSingleEventOnTimeNode();

        MultiOngoingCommandDispatcher m_dispatcher;
        ScenarioPoint m_clickedPoint;

};

// impl is in CreateEventFromTimeNodeState.cpp
class CreateFromTimeNodeState : public CreationState
{
    public:
        CreateFromTimeNodeState(
                const ScenarioStateMachine& stateMachine,
                ObjectPath&& scenarioPath,
                iscore::CommandStack& stack,
                QState* parent);

    private:
        void createSingleEventOnTimeNode();

        void createEventFromEventOnNothing(const ScenarioStateMachine& stateMachine);
        void createEventFromEventOnTimenode();

        void createConstraintBetweenEvents();


        MultiOngoingCommandDispatcher m_dispatcher;

        ScenarioPoint m_clickedPoint;
};

class CreateFromStateState : public CreationState
{
    public:
        CreateFromStateState(
                const ScenarioStateMachine& stateMachine,
                ObjectPath&& scenarioPath,
                iscore::CommandStack& stack,
                QState* parent);

    private:
        void createSingleEventOnTimeNode();

        void createEventFromStateOnNothing(const ScenarioStateMachine& stateMachine);
        void createEventFromStateOnTimeNode();

        void createConstraintBetweenEvents();


        MultiOngoingCommandDispatcher m_dispatcher;
        ScenarioPoint m_clickedPoint;
};

