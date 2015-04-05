#include "CreationToolState.hpp"


CreationToolState::CreationToolState(ScenarioStateMachine& sm) :
    GenericToolState{sm}
{
    auto t1 = new ClickOnEvent_Transition(*m_baseState);
    t1->setTargetState(m_baseState);
    m_waitState->addTransition(t1);

    auto t2 = new ClickOnNothing_Transition(*m_baseState);
    t2->setTargetState(m_baseState);
    m_waitState->addTransition(t2);

    // TODO ClickOnTimeNode_Transition
}

void CreationToolState::on_scenarioPressed()
{
    // TODO if one day we're feeling hardcore, maybe
    // make a variadic template that can match the lambdas
    // to the correct behaviour ?
    mapTopItem(itemUnderMouse(m_sm.scenePoint),
               [&] (const auto& id)
    { ct_sm.postEvent(new ClickOnEvent_Event{id, m_sm.scenarioPoint}); },
    [&] (const auto& id)
    { ct_sm.postEvent(new ClickOnTimeNode_Event{id, m_sm.scenarioPoint}); },
    [&] (const auto& id)
    { ct_sm.postEvent(new ClickOnConstraint_Event{id, m_sm.scenarioPoint}); }, // TODO Unnecessary
    [&] ()
    { ct_sm.postEvent(new ClickOnNothing_Event{m_sm.scenarioPoint}); });
}

void CreationToolState::on_scenarioMoved()
{
    mapWithCollision(
                [&] (const auto& id)
    { ct_sm.postEvent(new MoveOnEvent_Event{id, m_sm.scenarioPoint}); },
    [&] (const auto& id)
    { ct_sm.postEvent(new MoveOnTimeNode_Event{id, m_sm.scenarioPoint}); },
    [&] ()
    { ct_sm.postEvent(new MoveOnNothing_Event{m_sm.scenarioPoint}); });
}

void CreationToolState::on_scenarioReleased()
{
    mapWithCollision(
                [&] (const auto& id)
    { ct_sm.postEvent(new ReleaseOnEvent_Event{id, m_sm.scenarioPoint}); },
    [&] (const auto& id)
    { ct_sm.postEvent(new ReleaseOnTimeNode_Event{id, m_sm.scenarioPoint}); },
    [&] ()
    { ct_sm.postEvent(new ReleaseOnNothing_Event{m_sm.scenarioPoint}); });
}
