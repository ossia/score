#include "CreationToolState.hpp"
#include <iscore/statemachine/StateMachineTools.hpp>
#include <Scenario/Process/Temporal/StateMachines/Transitions/NothingTransitions.hpp>
#include <Scenario/Process/Temporal/StateMachines/Transitions/EventTransitions.hpp>
#include <Scenario/Process/Temporal/StateMachines/Transitions/StateTransitions.hpp>
#include <Scenario/Process/Temporal/StateMachines/Transitions/TimeNodeTransitions.hpp>

#include <Scenario/Process/Temporal/StateMachines/Tools/States/ScenarioCreation_FromEvent.hpp>
#include <Scenario/Process/Temporal/StateMachines/Tools/States/ScenarioCreation_FromState.hpp>
#include <Scenario/Process/Temporal/StateMachines/Tools/States/ScenarioCreation_FromNothing.hpp>
#include <Scenario/Process/Temporal/StateMachines/Tools/States/ScenarioCreation_FromTimeNode.hpp>
#include <Scenario/Process/Temporal/StateMachines/ScenarioStateMachine.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioPresenter.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Event/EventPresenter.hpp>
#include <Scenario/Document/Event/EventView.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodePresenter.hpp>
#include <Scenario/Document/TimeNode/TimeNodeView.hpp>

#include <Scenario/Process/ScenarioModel.hpp>

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
    {

        // Here we have the logic for the creation in nothing
        // where we instead choose the latest state if selected
        if(auto state = furthestSelectedState(m_parentSM.model()))
        {
            if(m_parentSM.model().events.at(state->eventId()).date() < m_parentSM.scenarioPoint.date)
            {
                localSM().postEvent(new ClickOnState_Event{
                                        state->id(),
                                        m_parentSM.scenarioPoint});
                return;
            }
        }

        localSM().postEvent(new ClickOnNothing_Event{m_parentSM.scenarioPoint});

    });
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
    if(isStateActive(m_createFromEventState))
        return m_createFromEventState;
    if(isStateActive(m_createFromNothingState))
        return m_createFromNothingState;
    if(isStateActive(m_createFromStateState))
        return m_createFromStateState;
    if(isStateActive(m_createFromTimeNodeState))
        return m_createFromTimeNodeState;
    return nullptr;
}
