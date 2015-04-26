#include "MoveToolState.hpp"
#include "Process/Temporal/StateMachines/StateMachineCommon.hpp"
#include "States/MoveStates.hpp"

MoveToolState::MoveToolState(ScenarioStateMachine& sm) :
    GenericToolState{sm}
{
    m_waitState = new QState;
    m_localSM.addState(m_waitState);
    m_localSM.setInitialState(m_waitState);

    /// Constraint
    /// //TODO remove useless arguments to ctor
    m_moveConstraint =
            new MoveConstraintState{
                  m_sm,
                  iscore::IDocument::path(m_sm.model()),
                  m_sm.commandStack(),
                  m_sm.locker(),
                  nullptr};

    make_transition<ClickOnConstraint_Transition>(m_waitState,
                                                  m_moveConstraint,
                                                  *m_moveConstraint);
    m_moveConstraint->addTransition(m_moveConstraint,
                                    SIGNAL(finished()),
                                    m_waitState);
    m_localSM.addState(m_moveConstraint);


    /// Event
    m_moveEvent =
            new MoveEventState{
                  m_sm,
                  iscore::IDocument::path(m_sm.model()),
                  m_sm.commandStack(),
                  m_sm.locker(),
                  nullptr};

    make_transition<ClickOnEvent_Transition>(m_waitState,
                                             m_moveEvent,
                                             *m_moveEvent);
    m_moveEvent->addTransition(m_moveEvent,
                               SIGNAL(finished()),
                               m_waitState);
    m_localSM.addState(m_moveEvent);


    /// TimeNode
    m_moveTimeNode =
            new MoveTimeNodeState{
                  m_sm,
                  iscore::IDocument::path(m_sm.model()),
                  m_sm.commandStack(),
                  m_sm.locker(),
                  nullptr};

    make_transition<ClickOnTimeNode_Transition>(m_waitState,
                                             m_moveTimeNode,
                                             *m_moveTimeNode);
    m_moveTimeNode->addTransition(m_moveTimeNode,
                                  SIGNAL(finished()),
                                  m_waitState);
    m_localSM.addState(m_moveTimeNode);
}

// TODO check that this is the correct state machine that receives
// the event when we have hierarchy.
void MoveToolState::on_scenarioPressed()
{
    mapTopItem(itemUnderMouse(m_sm.scenePoint),
    [&] (const id_type<EventModel>& id)
    { m_localSM.postEvent(new ClickOnEvent_Event{id, m_sm.scenarioPoint}); },
    [&] (const id_type<TimeNodeModel>& id)
    { m_localSM.postEvent(new ClickOnTimeNode_Event{id, m_sm.scenarioPoint}); },
    [&] (const id_type<ConstraintModel>& id)
    { m_localSM.postEvent(new ClickOnConstraint_Event{id, m_sm.scenarioPoint}); },
    [&] () { });
}

void MoveToolState::on_scenarioMoved()
{
    mapTopItem(itemUnderMouse(m_sm.scenePoint),
    [&] (const id_type<EventModel>& id)
    { m_localSM.postEvent(new MoveOnEvent_Event{id, m_sm.scenarioPoint}); },
    [&] (const id_type<TimeNodeModel>& id)
    { m_localSM.postEvent(new MoveOnTimeNode_Event{id, m_sm.scenarioPoint}); },
    [&] (const id_type<ConstraintModel>& id)
    { m_localSM.postEvent(new MoveOnConstraint_Event{id, m_sm.scenarioPoint}); },
    [&] ()
    { m_localSM.postEvent(new MoveOnNothing_Event{m_sm.scenarioPoint}); });
}

void MoveToolState::on_scenarioReleased()
{
    mapTopItem(itemUnderMouse(m_sm.scenePoint),
    [&] (const id_type<EventModel>& id)
    { m_localSM.postEvent(new ReleaseOnEvent_Event{id, m_sm.scenarioPoint}); },
    [&] (const id_type<TimeNodeModel>& id)
    { m_localSM.postEvent(new ReleaseOnTimeNode_Event{id, m_sm.scenarioPoint}); },
    [&] (const id_type<ConstraintModel>& id)
    { m_localSM.postEvent(new ReleaseOnConstraint_Event{id, m_sm.scenarioPoint}); },
    [&] ()
    { m_localSM.postEvent(new ReleaseOnNothing_Event{m_sm.scenarioPoint}); });
}
