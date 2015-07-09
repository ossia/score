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
                const ScenarioStateMachine& sm,
                iscore::CommandStack& stack,
                ObjectPath&& scenarioPath,
                QState* parent):
            CreationState{std::forward<ObjectPath>(scenarioPath), parent},
            m_parentSM{sm},
            m_dispatcher{stack}
        {

        }

    protected:
        void createToState_base(const id_type<StateModel>&);

        void createToEvent_base(const id_type<StateModel> &);

        void createToTimeNode_base(const id_type<StateModel> &);

        void createToNothing_base(const id_type<StateModel> &);


        template<typename DestinationState, typename Function>
        void add_transition(QState* from, DestinationState* to, Function&& fun)
        {
            using transition_type = ScenarioTransition_T<DestinationState::value()>;
            auto trans = make_transition<transition_type>(from, to, *this);
            trans->setObjectName(QString::number(DestinationState::value()));
            connect(trans,
                    &transition_type::triggered,
                    this, fun);
        }

        void rollback() { m_dispatcher.rollback<ScenarioRollbackStrategy>(); clearCreatedIds(); }

        const ScenarioStateMachine& m_parentSM;
        MultiOngoingCommandDispatcher m_dispatcher;

        ScenarioPoint m_clickedPoint;
};
