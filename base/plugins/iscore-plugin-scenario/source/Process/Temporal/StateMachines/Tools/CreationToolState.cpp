#include "CreationToolState.hpp"
#include "Process/Temporal/StateMachines/Transitions/NothingTransitions.hpp"
#include "Process/Temporal/StateMachines/Transitions/EventTransitions.hpp"
#include "Process/Temporal/StateMachines/Transitions/StateTransitions.hpp"
#include "Process/Temporal/StateMachines/Transitions/TimeNodeTransitions.hpp"

#include "Process/Temporal/StateMachines/Tools/States/ScenarioCreation_FromEvent.hpp"
#include "Process/Temporal/StateMachines/Tools/States/ScenarioCreation_FromState.hpp"
#include "Process/Temporal/StateMachines/Tools/States/ScenarioCreation_FromNothing.hpp"
#include "Process/Temporal/StateMachines/Tools/States/ScenarioCreation_FromTimeNode.hpp"
#include "Process/Temporal/StateMachines/ScenarioStateMachine.hpp"
#include "Process/Temporal/TemporalScenarioPresenter.hpp"
#include "Document/Event/EventModel.hpp"
#include "Document/Event/EventPresenter.hpp"
#include "Document/Event/EventView.hpp"
#include "Document/TimeNode/TimeNodeModel.hpp"
#include "Document/TimeNode/TimeNodePresenter.hpp"
#include "Document/TimeNode/TimeNodeView.hpp"

#include "Process/ScenarioModel.hpp"

#include <iscore/document/DocumentInterface.hpp>

CreationToolState::CreationToolState(ScenarioStateMachine& sm) :
    ScenarioTool{sm, &sm}
{
    m_waitState = new QState;
    localSM().addState(m_waitState);
    localSM().setInitialState(m_waitState);

    Path<ScenarioModel> scenarioPath = m_parentSM.model();

    //// Create from nothing ////
    m_createFromNothingState = new ScenarioCreation_FromNothing{
            m_parentSM,
            scenarioPath,
            m_parentSM.commandStack(), nullptr};

    make_transition<ClickOnNothing_Transition>(m_waitState, m_createFromNothingState, *m_createFromNothingState);
    m_createFromNothingState->addTransition(m_createFromNothingState, SIGNAL(finished()), m_waitState);

    localSM().addState(m_createFromNothingState);


    //// Create from an event ////
    m_createFromEventState = new ScenarioCreation_FromEvent{
            m_parentSM,
            scenarioPath,
            m_parentSM.commandStack(), nullptr};

    make_transition<ClickOnEvent_Transition>(m_waitState, m_createFromEventState, *m_createFromEventState);
    m_createFromEventState->addTransition(m_createFromEventState, SIGNAL(finished()), m_waitState);

    localSM().addState(m_createFromEventState);


    //// Create from a timenode ////
    m_createFromTimeNodeState = new ScenarioCreation_FromTimeNode{
            m_parentSM,
            scenarioPath,
            m_parentSM.commandStack(), nullptr};

    make_transition<ClickOnTimeNode_Transition>(m_waitState,
                                                m_createFromTimeNodeState,
                                                *m_createFromTimeNodeState);
    m_createFromTimeNodeState->addTransition(m_createFromTimeNodeState, SIGNAL(finished()), m_waitState);

    localSM().addState(m_createFromTimeNodeState);


    //// Create from a State ////
    m_createFromStateState = new ScenarioCreation_FromState{
            m_parentSM,
            scenarioPath,
            m_parentSM.commandStack(), nullptr};

    make_transition<ClickOnState_Transition>(m_waitState,
                                             m_createFromStateState,
                                             *m_createFromStateState);

    m_createFromStateState->addTransition(m_createFromStateState, SIGNAL(finished()), m_waitState);

    localSM().addState(m_createFromStateState);

}

void CreationToolState::on_pressed()
{
    mapTopItem(itemUnderMouse(m_parentSM.scenePoint),

    // Press a state
    [&] (const Id<StateModel>& id)
    { localSM().postEvent(new ClickOnState_Event{id, m_parentSM.scenarioPoint}); },

    // Press an event
    [&] (const Id<EventModel>& id)
    { localSM().postEvent(new ClickOnEvent_Event{id, m_parentSM.scenarioPoint}); },

    // Press a TimeNode
    [&] (const Id<TimeNodeModel>& id)
    { localSM().postEvent(new ClickOnTimeNode_Event{id, m_parentSM.scenarioPoint}); },

    // Press a Constraint
    [&] (const Id<ConstraintModel>&)
    { },

    // Press a slot handle
    [&] (const SlotModel&)
    { },

    // Click on the background
    [&] ()
    { localSM().postEvent(new ClickOnNothing_Event{m_parentSM.scenarioPoint}); });
}

void CreationToolState::on_moved()
{
    if(auto cs = currentState())
    {
        mapWithCollision(
                    [&] (const Id<StateModel>& id)
        { localSM().postEvent(new MoveOnState_Event{id, m_parentSM.scenarioPoint}); },
        [&] (const Id<EventModel>& id)
        { localSM().postEvent(new MoveOnEvent_Event{id, m_parentSM.scenarioPoint}); },
        [&] (const Id<TimeNodeModel>& id)
        { localSM().postEvent(new MoveOnTimeNode_Event{id, m_parentSM.scenarioPoint}); },
        [&] ()
        { localSM().postEvent(new MoveOnNothing_Event{m_parentSM.scenarioPoint}); },
        cs->createdStates,
        cs->createdEvents,
        cs->createdTimeNodes);

    }
}

void CreationToolState::on_released()
{
    if(auto cs = currentState())
    {
        mapWithCollision(
                    [&] (const Id<StateModel>& id)
        { localSM().postEvent(new ReleaseOnState_Event{id, m_parentSM.scenarioPoint}); },
        [&] (const Id<EventModel>& id)
        { localSM().postEvent(new ReleaseOnEvent_Event{id, m_parentSM.scenarioPoint}); },
        [&] (const Id<TimeNodeModel>& id)
        { localSM().postEvent(new ReleaseOnTimeNode_Event{id, m_parentSM.scenarioPoint}); },
        [&] ()
        { localSM().postEvent(new ReleaseOnNothing_Event{m_parentSM.scenarioPoint}); },
        cs->createdStates,
        cs->createdEvents,
        cs->createdTimeNodes);
    }
}

QList<Id<StateModel> > CreationToolState::getCollidingStates(const QVector<Id<StateModel> > &createdStates)
{
    return getCollidingModels(
                m_parentSM.presenter().states(),
                createdStates,
                m_parentSM.scenePoint);
}

QList<Id<EventModel>> CreationToolState::getCollidingEvents(const QVector<Id<EventModel>>& createdEvents)
{
    return getCollidingModels(
                m_parentSM.presenter().events(),
                createdEvents,
                m_parentSM.scenePoint);
}

QList<Id<TimeNodeModel>> CreationToolState::getCollidingTimeNodes(const QVector<Id<TimeNodeModel>>& createdTimeNodes)
{
    return getCollidingModels(
                m_parentSM.presenter().timeNodes(),
                createdTimeNodes,
                m_parentSM.scenePoint);
}

CreationState* CreationToolState::currentState() const
{
    if(m_createFromEventState->active())
        return m_createFromEventState;
    if(m_createFromNothingState->active())
        return m_createFromNothingState;
    if(m_createFromStateState->active())
        return m_createFromStateState;
    if(m_createFromTimeNodeState->active())
        return m_createFromTimeNodeState;
    return nullptr;
}
