#include "CreationToolState.hpp"

#include "States/CreateEventState.hpp"

CreationToolState::CreationToolState(const ScenarioStateMachine& sm) :
    GenericToolState{sm}
{
    m_baseState = new CreateEventState{
                    iscore::IDocument::path(m_sm.model()),
                    m_sm.commandStack(), nullptr};


    // Wait -> ...
    make_transition<ClickOnEvent_Transition>(m_waitState, m_baseState, *m_baseState);
    auto t_click_nothing = make_transition<ClickOnNothing_Transition>(m_waitState,
                                                                      m_baseState,
                                                                      *m_baseState);
    connect(t_click_nothing, &QAbstractTransition::triggered, [&] ()
    { m_baseState->clickedEvent = id_type<EventModel>(0); });

    // On finish
    m_baseState->addTransition(m_baseState, SIGNAL(finished()), m_waitState);

    m_localSM.addState(m_baseState);

    // TODO ClickOnTimeNode_Transition
}

void CreationToolState::on_scenarioPressed()
{
    // TODO Do some checking here to see if we're in the bounds of the scenario.

    // TODO if one day we're feeling hardcore, maybe
    // make a variadic template that can match the lambdas
    // to the correct behaviour ?
    mapTopItem(itemUnderMouse(m_sm.scenePoint),
               [&] (const auto& id)
    { m_localSM.postEvent(new ClickOnEvent_Event{id, m_sm.scenarioPoint}); },
    [&] (const auto& id)
    { m_localSM.postEvent(new ClickOnTimeNode_Event{id, m_sm.scenarioPoint}); },
    [&] (const auto& id)
    { m_localSM.postEvent(new ClickOnConstraint_Event{id, m_sm.scenarioPoint}); }, // TODO Unnecessary
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
    { m_localSM.postEvent(new MoveOnNothing_Event{m_sm.scenarioPoint}); });
}

void CreationToolState::on_scenarioReleased()
{
    mapWithCollision(
                [&] (const auto& id)
    { m_localSM.postEvent(new ReleaseOnEvent_Event{id, m_sm.scenarioPoint}); },
    [&] (const auto& id)
    { m_localSM.postEvent(new ReleaseOnTimeNode_Event{id, m_sm.scenarioPoint}); },
    [&] ()
    { m_localSM.postEvent(new ReleaseOnNothing_Event{m_sm.scenarioPoint}); });
}
