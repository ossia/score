#pragma once
#include <iscore/command/Dispatchers/MultiOngoingCommandDispatcher.hpp>
#include <Scenario/Commands/Scenario/Creations/CreationMetaCommand.hpp>
#include <Scenario/Process/Temporal/StateMachines/ScenarioStateMachineBaseStates.hpp>
#include <Scenario/Process/Temporal/StateMachines/Tools/ScenarioRollbackStrategy.hpp>
#include <Scenario/Process/Temporal/StateMachines/ScenarioStateMachineBaseTransitions.hpp>

template<int Value>
class StrongQState : public QState
{
    public:
        static constexpr auto value() { return Value; }
        StrongQState(QState* parent):
            QState{parent}
        {
            this->setObjectName(debug_StateMachineIDs<Value>());
        }
};
class ScenarioStateMachine;

// Here to prevent pollution of the CreationState header with the command dispatcher
class ScenarioCreationState : public CreationState
{
    public:
        ScenarioCreationState(
                const ScenarioStateMachine& sm,
                iscore::CommandStack& stack,
                const Path<ScenarioModel>& scenarioPath,
                QState* parent):
            CreationState{scenarioPath, parent},
            m_parentSM{sm},
            m_dispatcher{stack}
        {

        }

    protected:
        void createToState_base(const Id<StateModel>&);

        void createToEvent_base(const Id<StateModel> &);

        void createToTimeNode_base(const Id<StateModel> &);

        void createToNothing_base(const Id<StateModel> &);

        void makeSnapshot();


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
