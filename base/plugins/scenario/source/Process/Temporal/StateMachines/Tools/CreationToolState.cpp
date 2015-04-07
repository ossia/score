#include "CreationToolState.hpp"

#include "States/CreateEventState.hpp"

CreationToolState::CreationToolState(const ScenarioStateMachine& sm) :
    GenericToolState{sm}
{
    m_waitState = new QState;
    m_localSM.addState(m_waitState);
    m_localSM.setInitialState(m_waitState);

    /// Create from an event
    m_createFromEventState = new CreateFromEventState{
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
    /*
    auto createFromTimeNodeState = new CreateFromTimeNodeState{
                    iscore::IDocument::path(m_sm.model()),
                    m_sm.commandStack(), nullptr};
    make_transition<ClickOnTimeNode_Transition>(m_waitState,
                                                createFromTimeNodeState,
                                                *createFromTimeNodeState);
    createFromTimeNodeState->addTransition(createFromTimeNodeState, SIGNAL(finished()), m_waitState);
    m_localSM.addState(createFromTimeNodeState);
    */


}

void CreationToolState::on_scenarioPressed()
{
    mapTopItem(itemUnderMouse(m_sm.scenePoint),
               [&] (const auto& id)
    { m_localSM.postEvent(new ClickOnEvent_Event{id, m_sm.scenarioPoint}); },
    [&] (const auto& id)
    { m_localSM.postEvent(new ClickOnTimeNode_Event{id, m_sm.scenarioPoint}); },
    [&] (const auto& id)
    { },
    [&] ()
    { m_localSM.postEvent(new ClickOnNothing_Event{m_sm.scenarioPoint}); });
}

void CreationToolState::on_scenarioMoved()
{
    mapWithCollision(
                [&] (const auto& id)
    { m_localSM.postEvent(new MoveOnEvent_Event{id, m_sm.scenarioPoint}); },
    [&] (const auto& id)
    { m_localSM.postEvent(new MoveOnTimeNode_Event{id, m_sm.scenarioPoint}); },
    [&] ()
    { m_localSM.postEvent(new MoveOnNothing_Event{m_sm.scenarioPoint}); },
    m_createFromEventState->createdEvent(),
    m_createFromEventState->createdTimeNode());
}

void CreationToolState::on_scenarioReleased()
{
    mapWithCollision(
                [&] (const auto& id)
    { m_localSM.postEvent(new ReleaseOnEvent_Event{id, m_sm.scenarioPoint}); },
    [&] (const auto& id)
    { m_localSM.postEvent(new ReleaseOnTimeNode_Event{id, m_sm.scenarioPoint}); },
    [&] ()
    { m_localSM.postEvent(new ReleaseOnNothing_Event{m_sm.scenarioPoint}); },
    m_createFromEventState->createdEvent(),
    m_createFromEventState->createdTimeNode());
}
