#include "MoveToolState.hpp"
#include "States/MoveStates.hpp"

#include "Process/Temporal/StateMachines/Transitions/ConstraintTransitions.hpp"
#include "Process/Temporal/StateMachines/Transitions/EventTransitions.hpp"
#include "Process/Temporal/StateMachines/Transitions/TimeNodeTransitions.hpp"

#include "Process/Temporal/StateMachines/ScenarioStateMachine.hpp"
#include "Process/ScenarioModel.hpp"
MoveToolState::MoveToolState(ScenarioStateMachine& sm) :
    ScenarioToolState{sm}
{
    m_waitState = new QState;
    localSM().addState(m_waitState);
    localSM().setInitialState(m_waitState);

    /// Constraint
    /// //TODO remove useless arguments to ctor
    m_moveConstraint =
            new MoveConstraintState{
                  m_parentSM,
                  iscore::IDocument::path(m_parentSM.model()),
                  m_parentSM.commandStack(),
                  m_parentSM.locker(),
                  nullptr};

    make_transition<ClickOnConstraint_Transition>(m_waitState,
                                                  m_moveConstraint,
                                                  *m_moveConstraint);
    m_moveConstraint->addTransition(m_moveConstraint,
                                    SIGNAL(finished()),
                                    m_waitState);
    localSM().addState(m_moveConstraint);


    /// Event
    m_moveEvent =
            new MoveEventState{
                  m_parentSM,
                  iscore::IDocument::path(m_parentSM.model()),
                  m_parentSM.commandStack(),
                  m_parentSM.locker(),
                  nullptr};

    make_transition<ClickOnEvent_Transition>(m_waitState,
                                             m_moveEvent,
                                             *m_moveEvent);
    m_moveEvent->addTransition(m_moveEvent,
                               SIGNAL(finished()),
                               m_waitState);
    localSM().addState(m_moveEvent);


    /// TimeNode
    m_moveTimeNode =
            new MoveTimeNodeState{
                  m_parentSM,
                  iscore::IDocument::path(m_parentSM.model()),
                  m_parentSM.commandStack(),
                  m_parentSM.locker(),
                  nullptr};

    make_transition<ClickOnTimeNode_Transition>(m_waitState,
                                             m_moveTimeNode,
                                             *m_moveTimeNode);
    m_moveTimeNode->addTransition(m_moveTimeNode,
                                  SIGNAL(finished()),
                                  m_waitState);
    localSM().addState(m_moveTimeNode);
}

void MoveToolState::on_pressed()
{
    mapTopItem(itemUnderMouse(m_parentSM.scenePoint),
    [&] (const id_type<EventModel>& id)
    { localSM().postEvent(new ClickOnEvent_Event{id, m_parentSM.scenarioPoint}); },
    [&] (const id_type<TimeNodeModel>& id)
    { localSM().postEvent(new ClickOnTimeNode_Event{id, m_parentSM.scenarioPoint}); },
    [&] (const id_type<ConstraintModel>& id)
    { localSM().postEvent(new ClickOnConstraint_Event{id, m_parentSM.scenarioPoint}); },
    [&] () { });
}

void MoveToolState::on_moved()
{
    mapTopItem(itemUnderMouse(m_parentSM.scenePoint),
    [&] (const id_type<EventModel>& id)
    { localSM().postEvent(new MoveOnEvent_Event{id, m_parentSM.scenarioPoint}); },
    [&] (const id_type<TimeNodeModel>& id)
    { localSM().postEvent(new MoveOnTimeNode_Event{id, m_parentSM.scenarioPoint}); },
    [&] (const id_type<ConstraintModel>& id)
    { localSM().postEvent(new MoveOnConstraint_Event{id, m_parentSM.scenarioPoint}); },
    [&] ()
    { localSM().postEvent(new MoveOnNothing_Event{m_parentSM.scenarioPoint}); });
}

void MoveToolState::on_released()
{
    mapTopItem(itemUnderMouse(m_parentSM.scenePoint),
    [&] (const id_type<EventModel>& id)
    { localSM().postEvent(new ReleaseOnEvent_Event{id, m_parentSM.scenarioPoint}); },
    [&] (const id_type<TimeNodeModel>& id)
    { localSM().postEvent(new ReleaseOnTimeNode_Event{id, m_parentSM.scenarioPoint}); },
    [&] (const id_type<ConstraintModel>& id)
    { localSM().postEvent(new ReleaseOnConstraint_Event{id, m_parentSM.scenarioPoint}); },
    [&] ()
    { localSM().postEvent(new ReleaseOnNothing_Event{m_parentSM.scenarioPoint}); });
}
