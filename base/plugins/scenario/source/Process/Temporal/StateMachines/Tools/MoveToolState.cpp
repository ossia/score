#include "MoveToolState.hpp"
#include "Process/Temporal/StateMachines/StateMachineCommon.hpp"
#include "States/MoveStates.hpp"

MoveToolState::MoveToolState(ScenarioStateMachine& sm) :
    GenericToolState{sm}
{
    /// Constraint
    m_moveConstraintState =
            new MoveConstraintState{
                  iscore::IDocument::path(m_sm.model()),
                  m_sm.commandStack(),
                  m_sm.locker(),
                  nullptr};

    m_moveConstraintState->addTransition(m_moveConstraintState, SIGNAL(finished()), m_waitState);

    auto t_press_constraint = new ClickOnConstraint_Transition(*m_moveConstraintState);
    t_press_constraint->setTargetState(m_moveConstraintState);
    m_waitState->addTransition(t_press_constraint);
    m_localSM.addState(m_moveConstraintState);


    /// Event
    m_moveEventState =
            new MoveEventState{
                  iscore::IDocument::path(m_sm.model()),
                  m_sm.commandStack(),
                  m_sm.locker(),
                  nullptr};

    m_moveEventState->addTransition(m_moveEventState, SIGNAL(finished()), m_waitState);

    auto t_press_event = new ClickOnEvent_Transition(*m_moveEventState);
    t_press_event->setTargetState(m_moveEventState);
    m_waitState->addTransition(t_press_event);
    m_localSM.addState(m_moveEventState);


    /// TimeNode
    m_moveTimeNodeState =
            new MoveTimeNodeState{
                  iscore::IDocument::path(m_sm.model()),
                  m_sm.commandStack(),
                  m_sm.locker(),
                  nullptr};

    m_moveTimeNodeState->addTransition(m_moveTimeNodeState, SIGNAL(finished()), m_waitState);

    auto t_press_timenode = new ClickOnTimeNode_Transition(*m_moveTimeNodeState);
    t_press_timenode ->setTargetState(m_moveTimeNodeState);
    m_waitState->addTransition(t_press_timenode);
    m_localSM.addState(m_moveTimeNodeState);
}

void MoveToolState::on_scenarioPressed()
{
    mapTopItem(itemUnderMouse(m_sm.scenePoint),
    [&] (const auto& id)
    { m_localSM.postEvent(new ClickOnEvent_Event{id, m_sm.scenarioPoint}); },
    [&] (const auto& id)
    { m_localSM.postEvent(new ClickOnTimeNode_Event{id, m_sm.scenarioPoint}); },
    [&] (const auto& id)
    { m_localSM.postEvent(new ClickOnConstraint_Event{id, m_sm.scenarioPoint}); },
    [&] () { }); // TODO last case should not need to be here.
}

void MoveToolState::on_scenarioMoved()
{
    mapTopItem(itemUnderMouse(m_sm.scenePoint),
    [&] (const auto& id)
    { m_localSM.postEvent(new MoveOnEvent_Event{id, m_sm.scenarioPoint}); },
    [&] (const auto& id)
    { m_localSM.postEvent(new MoveOnTimeNode_Event{id, m_sm.scenarioPoint}); },
    [&] (const auto& id)
    { m_localSM.postEvent(new MoveOnConstraint_Event{id, m_sm.scenarioPoint}); },
    [&] ()
    { m_localSM.postEvent(new MoveOnNothing_Event{m_sm.scenarioPoint}); });
}

void MoveToolState::on_scenarioReleased()
{
    mapTopItem(itemUnderMouse(m_sm.scenePoint),
    [&] (const auto& id)
    { m_localSM.postEvent(new ReleaseOnEvent_Event{id, m_sm.scenarioPoint}); },
    [&] (const auto& id)
    { m_localSM.postEvent(new ReleaseOnTimeNode_Event{id, m_sm.scenarioPoint}); },
    [&] (const auto& id)
    { m_localSM.postEvent(new ReleaseOnConstraint_Event{id, m_sm.scenarioPoint}); },
    [&] ()
    { m_localSM.postEvent(new ReleaseOnNothing_Event{m_sm.scenarioPoint}); });
}
