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

namespace Scenario
{

class ToolPalette;


class CreationStateBase : public StateBase
{
    public:
        using StateBase::StateBase;

        QVector<Id<StateModel>> createdStates;
        QVector<Id<EventModel>> createdEvents;
        QVector<Id<TimeNodeModel>> createdTimeNodes;
        QVector<Id<ConstraintModel>> createdConstraints;

        void clearCreatedIds()
        {
            createdEvents.clear();
            createdConstraints.clear();
            createdTimeNodes.clear();
            createdStates.clear();
        }
};

// Here to prevent pollution of the CreationState header with the command dispatcher
class CreationState : public CreationStateBase
{
    public:
        CreationState(
                const Scenario::ToolPalette& sm,
                iscore::CommandStack& stack,
                const Path<ScenarioModel>& scenarioPath,
                QState* parent):
            CreationStateBase{scenarioPath, parent},
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
            using transition_type = Transition_T<DestinationState::value()>;
            auto trans = iscore::make_transition<transition_type>(from, to, *this);
            trans->setObjectName(QString::number(DestinationState::value()));
            connect(trans,
                    &transition_type::triggered,
                    this, fun);
        }

        void rollback() { m_dispatcher.rollback<ScenarioRollbackStrategy>(); clearCreatedIds(); }

        const Scenario::ToolPalette& m_parentSM;
        MultiOngoingCommandDispatcher m_dispatcher;

        Scenario::Point m_clickedPoint;
};

}
