#pragma once
#include <iscore/command/Dispatchers/MultiOngoingCommandDispatcher.hpp>
#include <Scenario/Commands/Scenario/Creations/CreationMetaCommand.hpp>
#include <Scenario/Process/Temporal/StateMachines/ScenarioStateMachineBaseStates.hpp>
#include <Scenario/Process/Temporal/StateMachines/Tools/ScenarioRollbackStrategy.hpp>
#include <Scenario/Process/Temporal/StateMachines/ScenarioStateMachineBaseTransitions.hpp>

#include <Scenario/Tools/elementFindingHelper.hpp>

#include <Scenario/Commands/Scenario/Creations/CreateConstraint.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateConstraint_State.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateConstraint_State_Event.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateConstraint_State_Event_TimeNode.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateSequence.hpp>

#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <Scenario/Commands/State/UpdateState.hpp>


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
template<typename Scenario_T, typename ToolPalette_T>
class CreationState : public CreationStateBase
{
    public:
        CreationState(
                const ToolPalette_T& sm,
                iscore::CommandStack& stack,
                const Path<Scenario_T>& scenarioPath,
                QState* parent):
            CreationStateBase{scenarioPath, parent},
            m_parentSM{sm},
            m_dispatcher{stack}
        {

        }

    protected:
        void createToState_base(const Id<StateModel>& originalState)
        {
            // make sure the hovered corresponding timenode dont have a date prior to original state date
            if(getDate(m_parentSM.model(), originalState) < getDate(m_parentSM.model(), hoveredState) )
            {
                auto cmd = new Scenario::Command::CreateConstraint{
                        Path<Scenario_T>{m_scenarioPath},
                        originalState,
                        hoveredState};

                m_dispatcher.submitCommand(cmd);

                createdConstraints.append(cmd->createdConstraint());
            }//else do nothing
        }

        void createToEvent_base(const Id<StateModel> & originalState)
        {
            // make sure the hovered corresponding timenode dont have a date prior to original state date
            if(getDate(m_parentSM.model(), originalState) < getDate(m_parentSM.model(), hoveredEvent) )
            {
                auto cmd = new Scenario::Command::CreateConstraint_State{
                        Path<Scenario_T>{m_scenarioPath},
                        originalState,
                        hoveredEvent,
                        currentPoint.y};

                m_dispatcher.submitCommand(cmd);

                createdConstraints.append(cmd->createdConstraint());
                createdStates.append(cmd->createdState());
            }//else do nothing
        }

        void createToTimeNode_base(const Id<StateModel> & originalState)
        {
            // make sure the hovered corresponding timenode dont have a date prior to original state date
            if(getDate(m_parentSM.model(), originalState) < getDate(m_parentSM.model(), hoveredTimeNode) )
            {
                auto cmd = new Scenario::Command::CreateConstraint_State_Event{
                        m_scenarioPath,
                        originalState,
                        hoveredTimeNode,
                        currentPoint.y};

                m_dispatcher.submitCommand(cmd);

                createdStates.append(cmd->createdState());
                createdEvents.append(cmd->createdEvent());
                createdConstraints.append(cmd->createdConstraint());
            }
        }

        void createToNothing_base(const Id<StateModel> & originalState)
        {
            auto create = [&] (auto cmd) {
                m_dispatcher.submitCommand(cmd);

                createdStates.append(cmd->createdState());
                createdEvents.append(cmd->createdEvent());
                createdTimeNodes.append(cmd->createdTimeNode());
                createdConstraints.append(cmd->createdConstraint());
            };

            if(!m_parentSM.editionSettings().sequence())
            {
                create(new Scenario::Command::CreateConstraint_State_Event_TimeNode{
                           m_scenarioPath,
                           originalState, // Put there in createInitialState
                           currentPoint.date,
                           currentPoint.y});
            }
            else
            {
                create(new Scenario::Command::CreateSequence{
                           m_scenarioPath,
                           originalState, // Put there in createInitialState
                           currentPoint.date,
                           currentPoint.y});
            }
        }

        void makeSnapshot()
        {
            if(createdStates.empty())
                return;

            if(!createdConstraints.empty())
            {
                const auto& cst = m_parentSM.model().constraints.at(createdConstraints.last());
                if(!cst.processes.empty())
                {
                    // In case of the presence of a sequence, we
                    // only use the sequence's namespace, hence we don't need to make a snapshot at the end..
                    return;
                }
            }

            auto doc = iscore::IDocument::documentFromObject(m_parentSM.model());
            auto device_explorer = doc->model().template pluginModel<DeviceDocumentPlugin>()->updateProxy.deviceExplorer;

            iscore::MessageList messages = getSelectionSnapshot(*device_explorer);
            if(messages.empty())
                return;

            m_dispatcher.submitCommand(new AddMessagesToState{
                                           m_parentSM.model().states.at(createdStates.last()).messages(),
                                           messages});
        }


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

        const ToolPalette_T& m_parentSM;
        MultiOngoingCommandDispatcher m_dispatcher;

        Scenario::Point m_clickedPoint;
};

}
