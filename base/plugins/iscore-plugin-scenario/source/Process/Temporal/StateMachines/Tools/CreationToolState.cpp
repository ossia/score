#include "CreationToolState.hpp"
#include "Process/Temporal/StateMachines/Transitions/NothingTransitions.hpp"
#include "Process/Temporal/StateMachines/Transitions/EventTransitions.hpp"
#include "Process/Temporal/StateMachines/Transitions/StateTransitions.hpp"
#include "Process/Temporal/StateMachines/Transitions/TimeNodeTransitions.hpp"

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

    // TODO state : create from a state
    /// Create from an event
    m_createFromEventState = new CreateFromEventState{
            m_parentSM,
            iscore::IDocument::path(m_parentSM.model()),
            m_parentSM.commandStack(), nullptr};

    make_transition<ClickOnEvent_Transition>(m_waitState, m_createFromEventState, *m_createFromEventState);
    m_createFromEventState->addTransition(m_createFromEventState, SIGNAL(finished()), m_waitState);

    // Creating from nothing is like creating from the start event.
    auto t_click_nothing = make_transition<ClickOnNothing_Transition>(m_waitState,
                                                                      m_createFromEventState,
                                                                      *m_createFromEventState);
    connect(t_click_nothing, &QAbstractTransition::triggered, [&] ()
    { m_createFromEventState->clickedEvent = m_parentSM.model().startEvent().id(); });
    localSM().addState(m_createFromEventState);

    /// Create from a timenode
    m_createFromTimeNodeState = new CreateFromTimeNodeState{
            m_parentSM,
            iscore::IDocument::path(m_parentSM.model()),
            m_parentSM.commandStack(), nullptr};
    ISCORE_TODO
    /*
    make_transition<ClickOnTimeNode_Transition>(m_waitState,
                                                m_createFromTimeNodeState,
                                                *m_createFromTimeNodeState);
    m_createFromTimeNodeState->addTransition(m_createFromTimeNodeState, SIGNAL(finished()), m_waitState);
    */

    localSM().addState(m_createFromTimeNodeState);

    /// Create from a State
    m_createFromStateState = new CreateFromStateState{
            m_parentSM,
            iscore::IDocument::path(m_parentSM.model()),
            m_parentSM.commandStack(), nullptr};
    ISCORE_TODO
    /*
    make_transition<ClickOnState_Transition>(m_waitState,
                                             m_createFromStateState,
                                             *m_createFromStateState);

    m_createFromStateState->addTransition(m_createFromStateState, SIGNAL(finished()), m_waitState);
    */
    localSM().addState(m_createFromStateState);

}

void CreationToolState::on_pressed()
{
    mapTopItem(itemUnderMouse(m_parentSM.scenePoint),

    // Press a state
    [&] (const id_type<StateModel>& id)
    { localSM().postEvent(new ClickOnState_Event{id, m_parentSM.scenarioPoint}); },

    // Press an event
    [&] (const id_type<EventModel>& id)
    { localSM().postEvent(new ClickOnEvent_Event{id, m_parentSM.scenarioPoint}); },

    // Press a TimeNode
    [&] (const id_type<TimeNodeModel>& id)
    { localSM().postEvent(new ClickOnTimeNode_Event{id, m_parentSM.scenarioPoint}); },

    // Press a Constraint
    [&] (const id_type<ConstraintModel>& id)
    { },

    // Click on the background
    [&] ()
    { localSM().postEvent(new ClickOnNothing_Event{m_parentSM.scenarioPoint}); });
}

void CreationToolState::on_moved()
{
    mapWithCollision(
    [&] (const id_type<StateModel>& id)
    { localSM().postEvent(new MoveOnState_Event{id, m_parentSM.scenarioPoint}); },
    [&] (const id_type<EventModel>& id)
    { localSM().postEvent(new MoveOnEvent_Event{id, m_parentSM.scenarioPoint}); },
    [&] (const id_type<TimeNodeModel>& id)
    { localSM().postEvent(new MoveOnTimeNode_Event{id, m_parentSM.scenarioPoint}); },
    [&] ()
    { localSM().postEvent(new MoveOnNothing_Event{m_parentSM.scenarioPoint}); },
    currentState().createdStates,
    currentState().createdEvents,
    currentState().createdTimeNodes);
}

void CreationToolState::on_released()
{
    mapWithCollision(
    [&] (const id_type<StateModel>& id)
    { localSM().postEvent(new ReleaseOnState_Event{id, m_parentSM.scenarioPoint}); },
    [&] (const id_type<EventModel>& id)
    { localSM().postEvent(new ReleaseOnEvent_Event{id, m_parentSM.scenarioPoint}); },
    [&] (const id_type<TimeNodeModel>& id)
    { localSM().postEvent(new ReleaseOnTimeNode_Event{id, m_parentSM.scenarioPoint}); },
    [&] ()
    { localSM().postEvent(new ReleaseOnNothing_Event{m_parentSM.scenarioPoint}); },
    currentState().createdStates,
    currentState().createdEvents,
    currentState().createdTimeNodes);
}

QList<id_type<StateModel> > CreationToolState::getCollidingStates(const QVector<id_type<StateModel> > &createdStates)
{
    return getCollidingModels(
                m_parentSM.presenter().states(),
                createdStates,
                m_parentSM.scenePoint);
}

QList<id_type<EventModel>> CreationToolState::getCollidingEvents(const QVector<id_type<EventModel>>& createdEvents)
{
    return getCollidingModels(
                m_parentSM.presenter().events(),
                createdEvents,
                m_parentSM.scenePoint);
}

QList<id_type<TimeNodeModel>> CreationToolState::getCollidingTimeNodes(const QVector<id_type<TimeNodeModel>>& createdTimeNodes)
{
    return getCollidingModels(
                m_parentSM.presenter().timeNodes(),
                createdTimeNodes,
                m_parentSM.scenePoint);
}

CreationState &CreationToolState::currentState() const
{
    if(m_createFromEventState->active())
        return *m_createFromEventState;
    if(m_createFromStateState->active())
        return *m_createFromStateState;
    if(m_createFromTimeNodeState->active())
        return *m_createFromTimeNodeState;

    Q_ASSERT(false);
}
