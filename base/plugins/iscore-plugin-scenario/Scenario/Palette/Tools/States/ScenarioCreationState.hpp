#pragma once
#include <iscore/command/Dispatchers/MultiOngoingCommandDispatcher.hpp>
#include <Scenario/Commands/Scenario/Creations/CreationMetaCommand.hpp>
#include <Scenario/Palette/ScenarioPaletteBaseStates.hpp>
#include <Scenario/Palette/Tools/ScenarioRollbackStrategy.hpp>
#include <Scenario/Palette/ScenarioPaletteBaseTransitions.hpp>

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
#include <Scenario/Commands/State/AddMessagesToState.hpp>


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
template<typename Scenario_T>
class CreationStateBase : public StateBase<Scenario_T>
{
    public:
        using StateBase<Scenario_T>::StateBase;

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
class CreationState : public CreationStateBase<Scenario_T>
{
    public:
        CreationState(
                const ToolPalette_T& sm,
                iscore::CommandStackFacade& stack,
                const Path<Scenario_T>& scenarioPath,
                QState* parent):
            CreationStateBase<Scenario_T>{scenarioPath, parent},
            m_parentSM{sm},
            m_dispatcher{stack}
        {

        }

    protected:
        void createToState_base(const Id<StateModel>& originalState)
        {
            // make sure the hovered corresponding timenode dont have a date prior to original state date
            if(getDate(m_parentSM.model(), originalState) < getDate(m_parentSM.model(), this->hoveredState) )
            {
                auto cmd = new Scenario::Command::CreateConstraint{
                        Path<Scenario_T>{this->m_scenarioPath},
                        originalState,
                        this->hoveredState};

                m_dispatcher.submitCommand(cmd);

                this->createdConstraints.append(cmd->createdConstraint());
            }//else do nothing
        }

        void createToEvent_base(const Id<StateModel> & originalState)
        {
            // make sure the hovered corresponding timenode dont have a date prior to original state date
            if(getDate(m_parentSM.model(), originalState) < getDate(m_parentSM.model(), this->hoveredEvent) )
            {
                auto cmd = new Scenario::Command::CreateConstraint_State{
                        Path<Scenario_T>{this->m_scenarioPath},
                        originalState,
                        this->hoveredEvent,
                        this->currentPoint.y};

                m_dispatcher.submitCommand(cmd);

                this->createdConstraints.append(cmd->createdConstraint());
                this->createdStates.append(cmd->createdState());
            }//else do nothing
        }

        void createToTimeNode_base(const Id<StateModel> & originalState)
        {
            // make sure the hovered corresponding timenode dont have a date prior to original state date
            if(getDate(m_parentSM.model(), originalState) < getDate(m_parentSM.model(), this->hoveredTimeNode) )
            {
                auto cmd = new Scenario::Command::CreateConstraint_State_Event{
                        this->m_scenarioPath,
                        originalState,
                        this->hoveredTimeNode,
                        this->currentPoint.y};

                m_dispatcher.submitCommand(cmd);

                this->createdStates.append(cmd->createdState());
                this->createdEvents.append(cmd->createdEvent());
                this->createdConstraints.append(cmd->createdConstraint());
            }
        }

        void createToNothing_base(const Id<StateModel> & originalState)
        {
            auto create = [&] (auto cmd) {
                m_dispatcher.submitCommand(cmd);

                this->createdStates.append(cmd->createdState());
                this->createdEvents.append(cmd->createdEvent());
                this->createdTimeNodes.append(cmd->createdTimeNode());
                this->createdConstraints.append(cmd->createdConstraint());
            };

            if(!m_parentSM.editionSettings().sequence())
            {
                create(new Scenario::Command::CreateConstraint_State_Event_TimeNode{
                           this->m_scenarioPath,
                           originalState, // Put there in createInitialState
                           this->currentPoint.date,
                           this->currentPoint.y});
            }
            else
            {
                create(new Scenario::Command::CreateSequence{
                           this->m_scenarioPath,
                           originalState, // Put there in createInitialState
                           this->currentPoint.date,
                           this->currentPoint.y});
            }
        }

        void makeSnapshot()
        {
            if(this->createdStates.empty())
                return;

            if(!this->createdConstraints.empty())
            {
                const auto& cst = m_parentSM.model().constraints.at(this->createdConstraints.last());
                if(!cst.processes.empty())
                {
                    // In case of the presence of a sequence, we
                    // only use the sequence's namespace, hence we don't need to make a snapshot at the end..
                    return;
                }
            }

            auto& doc = this->m_parentSM.context().document;
            auto device_explorer = doc.context().template plugin<DeviceDocumentPlugin>().updateProxy.deviceExplorer;

            iscore::MessageList messages = getSelectionSnapshot(*device_explorer);
            if(messages.empty())
                return;

            m_dispatcher.submitCommand(new AddMessagesToState{
                                           m_parentSM.model().states.at(this->createdStates.last()).messages(),
                                           messages});
        }


        template<typename DestinationState, typename Function>
        void add_transition(QState* from, DestinationState* to, Function&& fun)
        {
            using transition_type = Transition_T<Scenario_T, DestinationState::value()>;
            auto trans = iscore::make_transition<transition_type>(from, to, *this);
            trans->setObjectName(QString::number(DestinationState::value()));
            QObject::connect(trans, &transition_type::triggered,
                             this, fun);
        }

        void rollback()
        {
            m_dispatcher.template rollback<ScenarioRollbackStrategy>();
            this->clearCreatedIds();
        }

        const ToolPalette_T& m_parentSM;
        MultiOngoingCommandDispatcher m_dispatcher;

        Scenario::Point m_clickedPoint;
};

}
