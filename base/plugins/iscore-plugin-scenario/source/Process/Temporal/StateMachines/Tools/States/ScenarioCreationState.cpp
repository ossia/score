#include "ScenarioCreationState.hpp"

#include "Commands/Scenario/Creations/CreateConstraint.hpp"
#include "Commands/Scenario/Creations/CreateConstraint_State.hpp"
#include "Commands/Scenario/Creations/CreateConstraint_State_Event.hpp"
#include "Commands/Scenario/Creations/CreateConstraint_State_Event_TimeNode.hpp"

using namespace Scenario::Command;
void ScenarioCreationState::createToState_base(const Id<StateModel> & originalState)
{
    auto cmd = new CreateConstraint{
            Path<ScenarioModel>{m_scenarioPath},
            originalState,
            hoveredState};

    m_dispatcher.submitCommand(cmd);

    createdConstraints.append(cmd->createdConstraint());
}


void ScenarioCreationState::createToEvent_base(const Id<StateModel> & originalState)
{
    auto cmd = new CreateConstraint_State{
            Path<ScenarioModel>{m_scenarioPath},
            originalState,
            hoveredEvent,
            currentPoint.y};

    m_dispatcher.submitCommand(cmd);

    createdConstraints.append(cmd->createdConstraint());
    createdStates.append(cmd->createdState());
}


void ScenarioCreationState::createToTimeNode_base(const Id<StateModel> & originalState)
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


void ScenarioCreationState::createToNothing_base(const Id<StateModel> & originalState)
{
    auto cmd = new CreateConstraint_State_Event_TimeNode{
            m_scenarioPath,
            originalState, // Put there in createInitialState
            currentPoint.date,
            currentPoint.y};
    m_dispatcher.submitCommand(cmd);

    createdStates.append(cmd->createdState());
    createdEvents.append(cmd->createdEvent());
    createdTimeNodes.append(cmd->createdTimeNode());
    createdConstraints.append(cmd->createdConstraint());
}
