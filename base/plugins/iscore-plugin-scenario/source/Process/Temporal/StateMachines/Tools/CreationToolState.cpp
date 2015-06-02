#include "CreationToolState.hpp"
#include "Process/Temporal/StateMachines/Transitions/NothingTransitions.hpp"
#include "Process/Temporal/StateMachines/Transitions/EventTransitions.hpp"
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
CreationToolState::CreationToolState(const ScenarioStateMachine& sm) :
    ScenarioToolState{sm}
{
    m_waitState = new QState;
    localSM().addState(m_waitState);
    localSM().setInitialState(m_waitState);

    /// Create from an event
    m_createFromEventState = new CreateFromEventState{
            m_parentSM,
            iscore::IDocument::path(m_parentSM.model()),
            m_parentSM.commandStack(), nullptr};

    make_transition<ClickOnEvent_Transition>(m_waitState, m_createFromEventState, *m_createFromEventState);
    m_createFromEventState->addTransition(m_createFromEventState, SIGNAL(finished()), m_waitState);

    auto t_click_nothing = make_transition<ClickOnNothing_Transition>(m_waitState,
                                                                      m_createFromEventState,
                                                                      *m_createFromEventState);
    connect(t_click_nothing, &QAbstractTransition::triggered, [&] ()
    { m_createFromEventState->clickedEvent = id_type<EventModel>(0); });
    localSM().addState(m_createFromEventState);

    /// Create from a timenode
    m_createFromTimeNodeState = new CreateFromTimeNodeState{
            m_parentSM,
            iscore::IDocument::path(m_parentSM.model()),
            m_parentSM.commandStack(), nullptr};
    make_transition<ClickOnTimeNode_Transition>(m_waitState,
                                                m_createFromTimeNodeState,
                                                *m_createFromTimeNodeState);
    m_createFromTimeNodeState->addTransition(m_createFromTimeNodeState, SIGNAL(finished()), m_waitState);
    localSM().addState(m_createFromTimeNodeState);
}

void CreationToolState::on_pressed()
{
    mapTopItem(itemUnderMouse(m_parentSM.scenePoint),
    [&] (const id_type<EventModel>& id)
    { localSM().postEvent(new ClickOnEvent_Event{id, m_parentSM.scenarioPoint}); },
    [&] (const id_type<TimeNodeModel>& id)
    { localSM().postEvent(new ClickOnTimeNode_Event{id, m_parentSM.scenarioPoint}); },
    [&] (const id_type<ConstraintModel>& id)
    { },
    [&] ()
    { localSM().postEvent(new ClickOnNothing_Event{m_parentSM.scenarioPoint}); });
}

void CreationToolState::on_moved()
{
    auto currentState = [&] () -> const CreationState&
    {
            return m_createFromEventState->active()
            ? static_cast<const CreationState&>(*m_createFromEventState)
            : static_cast<const CreationState&>(*m_createFromTimeNodeState);
};
    mapWithCollision(
    [&] (const id_type<EventModel>& id)
    { localSM().postEvent(new MoveOnEvent_Event{id, m_parentSM.scenarioPoint}); },
    [&] (const id_type<TimeNodeModel>& id)
    { localSM().postEvent(new MoveOnTimeNode_Event{id, m_parentSM.scenarioPoint}); },
    [&] ()
    { localSM().postEvent(new MoveOnNothing_Event{m_parentSM.scenarioPoint}); },
    currentState().createdEvent(),
    currentState().createdTimeNode());
}

void CreationToolState::on_released()
{
    auto currentState = [&] () -> const CreationState&
    {
            return m_createFromEventState->active()
            ? static_cast<const CreationState&>(*m_createFromEventState)
            : static_cast<const CreationState&>(*m_createFromTimeNodeState);
};
    mapWithCollision(
    [&] (const id_type<EventModel>& id)
    { localSM().postEvent(new ReleaseOnEvent_Event{id, m_parentSM.scenarioPoint}); },
    [&] (const id_type<TimeNodeModel>& id)
    { localSM().postEvent(new ReleaseOnTimeNode_Event{id, m_parentSM.scenarioPoint}); },
    [&] ()
    { localSM().postEvent(new ReleaseOnNothing_Event{m_parentSM.scenarioPoint}); },
    currentState().createdEvent(),
    currentState().createdTimeNode());
}

QList<id_type<EventModel>> CreationToolState::getCollidingEvents(const id_type<EventModel> &createdEvent)
{
    return getCollidingModels(
                m_parentSM.presenter().events(),
                createdEvent,
                m_parentSM.scenePoint);
}

QList<id_type<TimeNodeModel>> CreationToolState::getCollidingTimeNodes(const id_type<TimeNodeModel> &createdTimeNode)
{
    return getCollidingModels(
                m_parentSM.presenter().timeNodes(),
                createdTimeNode,
                m_parentSM.scenePoint);
}
