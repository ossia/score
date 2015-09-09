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
        StrongQState(QState* parent):
            QState{parent}
        {
            QString txt;

            auto object = static_cast<ScenarioElement>(Value % 10);
            auto modifier = static_cast<Modifier_tagme>((Value - object) % 1000 / 100);
            switch(modifier)
            {
                case Modifier_tagme::Click:
                    txt += "Click on";
                    break;
                case Modifier_tagme::Move:
                    txt += "Move on";
                    break;
                case Modifier_tagme::Release:
                    txt += "Release on";
                    break;
            }

            switch(object)
            {
                case ScenarioElement::Nothing:
                    txt += "nothing";
                    break;
                case ScenarioElement::TimeNode:
                    txt += "TimeNode";
                    break;
                case ScenarioElement::Event:
                    txt += "Event";
                    break;
                case ScenarioElement::Constraint:
                    txt += "Constraint";
                    break;
                case ScenarioElement::State:
                    txt += "State";
                    break;
                case ScenarioElement::SlotOverlay_e:
                    txt += "SlotOverlay_e";
                    break;
                case ScenarioElement::SlotHandle_e:
                    txt += "SlotHandle_e";
                    break;
            }

            this->setObjectName(txt);
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
