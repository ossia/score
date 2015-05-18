#include "CreationToolState.hpp"


CreationToolState::CreationToolState(const ScenarioStateMachine& sm) :
    GenericToolState{sm}
{
    m_waitState = new QState;
    m_localSM.addState(m_waitState);
    m_localSM.setInitialState(m_waitState);

    /// Create from an event
    m_createFromEventState = new CreateFromEventState{
            m_sm,
            iscore::IDocument::path(m_sm.model()),
            m_sm.commandStack(), nullptr};

    make_transition<ClickOnEvent_Transition>(m_waitState, m_createFromEventState, *m_createFromEventState);
    m_createFromEventState->addTransition(m_createFromEventState, SIGNAL(finished()), m_waitState);

    auto t_click_nothing = make_transition<ClickOnNothing_Transition>(m_waitState,
                                                                      m_createFromEventState,
                                                                      *m_createFromEventState);
    connect(t_click_nothing, &QAbstractTransition::triggered, [&] ()
    { m_createFromEventState->clickedEvent = id_type<EventModel>(0); });
    m_localSM.addState(m_createFromEventState);

    /// Create from a timenode
    m_createFromTimeNodeState = new CreateFromTimeNodeState{
            m_sm,
            iscore::IDocument::path(m_sm.model()),
            m_sm.commandStack(), nullptr};
    make_transition<ClickOnTimeNode_Transition>(m_waitState,
                                                m_createFromTimeNodeState,
                                                *m_createFromTimeNodeState);
    m_createFromTimeNodeState->addTransition(m_createFromTimeNodeState, SIGNAL(finished()), m_waitState);
    m_localSM.addState(m_createFromTimeNodeState);
}

void CreationToolState::on_scenarioPressed()
{
    mapTopItem(itemUnderMouse(m_sm.scenePoint),
    [&] (const id_type<EventModel>& id)
    { m_localSM.postEvent(new ClickOnEvent_Event{id, m_sm.scenarioPoint}); },
    [&] (const id_type<TimeNodeModel>& id)
    { m_localSM.postEvent(new ClickOnTimeNode_Event{id, m_sm.scenarioPoint}); },
    [&] (const id_type<ConstraintModel>& id)
    { },
    [&] ()
    { m_localSM.postEvent(new ClickOnNothing_Event{m_sm.scenarioPoint}); });
}

void CreationToolState::on_scenarioMoved()
{
    auto currentState = [&] () -> const CreationState&
    {
            return m_createFromEventState->active()
            ? static_cast<const CreationState&>(*m_createFromEventState)
            : static_cast<const CreationState&>(*m_createFromTimeNodeState);
};
    mapWithCollision(
    [&] (const id_type<EventModel>& id)
    { m_localSM.postEvent(new MoveOnEvent_Event{id, m_sm.scenarioPoint}); },
    [&] (const id_type<TimeNodeModel>& id)
    { m_localSM.postEvent(new MoveOnTimeNode_Event{id, m_sm.scenarioPoint}); },
    [&] ()
    { m_localSM.postEvent(new MoveOnNothing_Event{m_sm.scenarioPoint}); },
    currentState().createdEvent(),
    currentState().createdTimeNode());
}

void CreationToolState::on_scenarioReleased()
{
    auto currentState = [&] () -> const CreationState&
    {
            return m_createFromEventState->active()
            ? static_cast<const CreationState&>(*m_createFromEventState)
            : static_cast<const CreationState&>(*m_createFromTimeNodeState);
};
    mapWithCollision(
    [&] (const id_type<EventModel>& id)
    { m_localSM.postEvent(new ReleaseOnEvent_Event{id, m_sm.scenarioPoint}); },
    [&] (const id_type<TimeNodeModel>& id)
    { m_localSM.postEvent(new ReleaseOnTimeNode_Event{id, m_sm.scenarioPoint}); },
    [&] ()
    { m_localSM.postEvent(new ReleaseOnNothing_Event{m_sm.scenarioPoint}); },
    currentState().createdEvent(),
    currentState().createdTimeNode());
}
