#include "CreationToolState.hpp"
#include <iscore/statemachine/StateMachineTools.hpp>
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
    {
        // Here we have the logic for the creation in nothing
        // where we instead choose the latest state if selected
        auto& scenar = m_parentSM.model();

        const StateModel* furthest_state{};
        {
            TimeValue max_t = TimeValue::zero();
            double max_y = 0;
            for(StateModel& state : scenar.states)
            {
                if(state.selection.get())
                {
                    auto date = scenar.events.at(state.eventId()).date();
                    if(!furthest_state || date > max_t)
                    {
                        max_t = date;
                        max_y = state.heightPercentage();
                        furthest_state = &state;
                    }
                    else if(date == max_t && state.heightPercentage() > max_y)
                    {
                        max_y = state.heightPercentage();
                        furthest_state = &state;
                    }
                }
            }
            if(furthest_state)
            {
                localSM().postEvent(new ClickOnState_Event{furthest_state->id(), m_parentSM.scenarioPoint});
                return;
            }
        }

        const ConstraintModel* furthest_constraint{};
        {
            TimeValue max_t = TimeValue::zero();
            double max_y = 0;
            // If there is no furthest state, we instead go for a constraint
            for(ConstraintModel& cst : scenar.constraints)
            {
                if(cst.selection.get())
                {
                    auto date = cst.duration.defaultDuration();
                    if(!furthest_constraint || date > max_t)
                    {
                        max_t = date;
                        max_y = cst.heightPercentage();
                        furthest_constraint = &cst;
                    }
                    else if (date == max_t && cst.heightPercentage() > max_y)
                    {
                        max_y = cst.heightPercentage();
                        furthest_constraint = &cst;
                    }
                }
            }

            if (furthest_constraint)
            {
                localSM().postEvent(new ClickOnState_Event{furthest_constraint->endState(), m_parentSM.scenarioPoint});
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
