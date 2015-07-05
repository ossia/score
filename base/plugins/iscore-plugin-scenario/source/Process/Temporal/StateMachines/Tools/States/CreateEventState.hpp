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

// TODO correctly separate in files
class CreateFromEventState : public ScenarioCreationState
{
    public:
        CreateFromEventState(
                const ScenarioStateMachine& stateMachine,
                ObjectPath&& scenarioPath,
                iscore::CommandStack& stack,
                QState* parent);

    private:
        void createInitialState();

        void createToNothing();
        void createToState();
        void createToEvent();
        void createToTimeNode();

        // TODO create single state
};

// impl is in CreateEventFromTimeNodeState.cpp
class CreateFromTimeNodeState : public ScenarioCreationState
{
    public:
        CreateFromTimeNodeState(
                const ScenarioStateMachine& stateMachine,
                ObjectPath&& scenarioPath,
                iscore::CommandStack& stack,
                QState* parent);

    private:
        void createSingleEventAndState();

        void createToNothing(const ScenarioStateMachine& stateMachine);
        void createToState();
        void createToEvent();
        void createToTimeNode();
};

class CreateFromStateState : public ScenarioCreationState
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
};

