#include "ScenarioCreationState.hpp"
#include "Process/Temporal/StateMachines/ScenarioStateMachine.hpp"
#include "Process/ScenarioModel.hpp"
#include <Tools/elementFindingHelper.hpp>

#include "Commands/Scenario/Creations/CreateConstraint.hpp"
#include "Commands/Scenario/Creations/CreateConstraint_State.hpp"
#include "Commands/Scenario/Creations/CreateConstraint_State_Event.hpp"
#include "Commands/Scenario/Creations/CreateConstraint_State_Event_TimeNode.hpp"
#include "Commands/Scenario/Creations/CreateSequence.hpp"

using namespace Scenario::Command;
void ScenarioCreationState::createToState_base(const Id<StateModel> & originalState)
{
    // make sure the hovered corresponding timenode dont have a date prior to original state date
    if(getDate(m_parentSM.model(), originalState) < getDate(m_parentSM.model(), hoveredState) )
    {
        auto cmd = new CreateConstraint{
                   Path<ScenarioModel>{m_scenarioPath},
                   originalState,
                   hoveredState};

        m_dispatcher.submitCommand(cmd);

        createdConstraints.append(cmd->createdConstraint());
    }//else do nothing
}


void ScenarioCreationState::createToEvent_base(const Id<StateModel> & originalState)
{
    // make sure the hovered corresponding timenode dont have a date prior to original state date
    if(getDate(m_parentSM.model(), originalState) < getDate(m_parentSM.model(), hoveredEvent) )
        {
        auto cmd = new CreateConstraint_State{
                Path<ScenarioModel>{m_scenarioPath},
                originalState,
                hoveredEvent,
                currentPoint.y};

        m_dispatcher.submitCommand(cmd);

        createdConstraints.append(cmd->createdConstraint());
        createdStates.append(cmd->createdState());
    }//else do nothing
}


void ScenarioCreationState::createToTimeNode_base(const Id<StateModel> & originalState)
{
    // make sure the hovered corresponding timenode dont have a date prior to original state date
    if(getDate(m_parentSM.model(), originalState) < getDate(m_parentSM.model(), hoveredTimeNode) )
    {
        auto cmd = new CreateConstraint_State_Event{
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


void ScenarioCreationState::createToNothing_base(const Id<StateModel> & originalState)
{
    auto create = [&] (auto cmd) {
        m_dispatcher.submitCommand(cmd);

        createdStates.append(cmd->createdState());
        createdEvents.append(cmd->createdEvent());
        createdTimeNodes.append(cmd->createdTimeNode());
        createdConstraints.append(cmd->createdConstraint());
    };

    if(!m_parentSM.isShiftPressed())
    {
        create(new CreateConstraint_State_Event_TimeNode{
                m_scenarioPath,
                originalState, // Put there in createInitialState
                currentPoint.date,
                currentPoint.y});
    }
    else
    {
        create(new CreateSequence{
                m_scenarioPath,
                originalState, // Put there in createInitialState
                currentPoint.date,
                currentPoint.y});
    }
}


#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <Plugin/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Plugin/Panel/DeviceExplorerModel.hpp>
#include <Commands/State/UpdateState.hpp>
void ScenarioCreationState::makeSnapshot()
{
    if(createdStates.empty())
        return;

    auto doc = iscore::IDocument::documentFromObject(m_parentSM.model());
    auto device_explorer = doc->model().pluginModel<DeviceDocumentPlugin>()->updateProxy.deviceExplorer;

    iscore::MessageList messages = getSelectionSnapshot(*device_explorer);
    if(messages.empty())
        return;

    m_dispatcher.submitCommand(new AddMessagesToState{
               m_parentSM.model().states.at(createdStates.last()).messages(),
               messages});
}
