#include "CreateEventState.hpp"
#include <Process/ScenarioModel.hpp>
#include <Document/TimeNode/TimeNodeModel.hpp>
#include <Document/Constraint/ConstraintModel.hpp>
#include <Document/Event/EventModel.hpp>

#include <Commands/Scenario/Displacement/MoveEvent.hpp>
#include "Commands/Scenario/Creations/CreateEventAfterEvent.hpp"
#include "Commands/Scenario/Creations/CreateEventAfterEventOnTimeNode.hpp"
#include "Commands/Scenario/Creations/CreateConstraint.hpp"
#include "Commands/Scenario/Displacement/MoveEvent.hpp"
#include "Commands/Scenario/Displacement/MoveTimeNode.hpp"
#include "Commands/Scenario/Displacement/MoveConstraint.hpp"
#include "Commands/TimeNode/MergeTimeNodes.hpp"

#include <QFinalState>

CreateState::CreateState(ObjectPath &&scenarioPath,
                                   iscore::CommandStack& stack,
                                   QState* parent):
    CommonState{std::move(scenarioPath), parent},
    m_dispatcher{stack, nullptr}
{
    using namespace Scenario::Command;
    auto finalState = new QFinalState{this};
    connect(finalState, &QState::entered, [&] ()
    {
        m_createdEvent = id_type<EventModel>{};
        m_createdTimeNode = id_type<TimeNodeModel>{};
    });

    QState* mainState = new QState{this};
    {
        QState* pressedState = new QState{mainState};
        QState* releasedState = new QState{mainState};
        QState* movingOnNothingState = new QState{mainState};
        QState* movingOnEventState = new QState{mainState};
        QState* movingOnTimeNodeState = new QState{mainState};

        // General setup
        mainState->setInitialState(pressedState);
        releasedState->addTransition(finalState);

        // Release
        make_transition<ReleaseOnAnything_Transition>(mainState, releasedState);

        // Pressed -> ...
        make_transition<MoveOnNothing_Transition>(
                    pressedState, movingOnNothingState, *this);

        /// MoveOnNothing -> ...
        // MoveOnNothing -> MoveOnNothing. Nothing particular to trigger.
        make_transition<MoveOnNothing_Transition>(
                    movingOnNothingState, movingOnNothingState, *this);

        // MoveOnNothing -> MoveOnEvent.
        auto t_move_nothing_event =
                make_transition<MoveOnEvent_Transition>(
                    movingOnNothingState, movingOnEventState, *this);

        connect(t_move_nothing_event, &MoveOnEvent_Transition::triggered,
                [&] () { m_dispatcher.rollback(); createConstraintBetweenEvents(); });

        // MoveOnNothing -> MoveOnTimeNode
        auto t_move_nothing_timenode =
                make_transition<MoveOnTimeNode_Transition>(
                    movingOnNothingState, movingOnTimeNodeState, *this);

        connect(t_move_nothing_timenode, &MoveOnEvent_Transition::triggered,
                [&] () {  m_dispatcher.rollback(); createEventFromEventOnTimeNode(); });


        /// MoveOnEvent -> ...
        // MoveOnEvent -> MoveOnNothing
        auto t_move_event_nothing =
                make_transition<MoveOnNothing_Transition>(
                    movingOnEventState, movingOnNothingState, *this);

        connect(t_move_event_nothing, &MoveOnEvent_Transition::triggered,
                [&] () {  m_dispatcher.rollback();  createEventFromEventOnNothing(); });

        // MoveOnEvent -> MoveOnEvent : nothing to do.

        // MoveOnEvent -> MoveOnTimeNode
        auto t_move_event_timenode =
                make_transition<MoveOnTimeNode_Transition>(
                    movingOnEventState, movingOnTimeNodeState, *this);

        connect(t_move_event_timenode, &MoveOnEvent_Transition::triggered,
                [&] () {  m_dispatcher.rollback(); createEventFromEventOnTimeNode(); });


        /// MoveOnTimeNode -> ...
        // MoveOnTimeNode -> MoveOnNothing
        auto t_move_timenode_nothing =
                make_transition<MoveOnNothing_Transition>(
                    movingOnTimeNodeState, movingOnNothingState, *this);

        connect(t_move_timenode_nothing, &MoveOnEvent_Transition::triggered,
                [&] () {  m_dispatcher.rollback(); createEventFromEventOnNothing(); });


        // MoveOnTimeNode -> MoveOnEvent
        auto t_move_timenode_event =
                make_transition<MoveOnEvent_Transition>(
                    movingOnTimeNodeState, movingOnEventState, *this);

        connect(t_move_timenode_event, &MoveOnEvent_Transition::triggered,
                [&] () {  m_dispatcher.rollback(); createConstraintBetweenEvents(); });


        // MoveOnTimeNode -> MoveOnTimeNode
        make_transition<MoveOnTimeNode_Transition>(
                    movingOnTimeNodeState, movingOnTimeNodeState, *this);

        // What happens in each state.
        QObject::connect(pressedState, &QState::entered,
                         this, &CreateState::createEventFromEventOnNothing);

        QObject::connect(movingOnNothingState, &QState::entered, [&] ()
        {
            m_dispatcher.submitCommand(
                        new MoveEvent{
                            ObjectPath{m_scenarioPath},
                            m_createdEvent,
                            point.date,
                            point.y});
        });

        QObject::connect(movingOnTimeNodeState, &QState::entered, [&] ()
        {
            m_dispatcher.submitCommand(
                        new MoveEvent{
                            ObjectPath{m_scenarioPath},
                            m_createdEvent,
                            m_scenarioPath.find<ScenarioModel>()->timeNode(hoveredTimeNode)->date(),
                            point.y});
        });

        QObject::connect(releasedState, &QState::entered, [&] ()
        {
            m_dispatcher.commit();
        });
    }

    QState* rollbackState = new QState{this};
    make_transition<Cancel_Transition>(mainState, rollbackState);
    rollbackState->addTransition(finalState);
    QObject::connect(rollbackState, &QState::entered, [&] ()
    {
        m_dispatcher.rollback();
    });

    setInitialState(mainState);
}


// Note : clickedEvent is set at startEvent if clicking in the background.
void CreateState::createEventFromEventOnNothing()
{
    auto init = new Scenario::Command::CreateEventAfterEvent{
                ObjectPath{m_scenarioPath},
                clickedEvent,
                point.date,
                point.y};
    m_createdEvent = init->createdEvent();
    m_createdTimeNode = init->createdTimeNode();

    m_dispatcher.submitCommand(init);
}

void CreateState::createEventFromEventOnTimeNode()
{
    auto cmd = new Scenario::Command::CreateEventAfterEventOnTimeNode(
                   ObjectPath{m_scenarioPath},
                   clickedEvent,
                   hoveredTimeNode,
                   point.date,
                   point.y);

    m_createdEvent = cmd->createdEvent();
    m_createdTimeNode = id_type<TimeNodeModel>{};
    m_dispatcher.submitCommand(cmd);
}


void CreateState::createEventFromTimeNodeOnNothing()
{
    // TODO Faire CreateEventAfterTimeNode
    // Macro : CreateEventOnTimeNode, followed by CreateEventAfterEvent[OnTimeNode], followed by Move...;
    // Maybe use a set of ObjectIdentifier for the createdEvents / timenodes ??
}

void CreateState::createEventFromTimeNodeOnTimeNode()
{
    // TODO
}


void CreateState::createConstraintBetweenEvents()
{
    auto cmd = new Scenario::Command::CreateConstraint(
                   ObjectPath{m_scenarioPath},
                   clickedEvent,
                   hoveredEvent);

    m_dispatcher.submitCommand(cmd);
}

