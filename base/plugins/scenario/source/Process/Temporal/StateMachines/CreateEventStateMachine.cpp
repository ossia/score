#include "CreateEventStateMachine.hpp"
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

CreateEventState::CreateEventState(ObjectPath &&scenarioPath,
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
        auto t_release = new ReleaseOnAnything_Transition;
        t_release->setTargetState(releasedState);
        mainState->addTransition(t_release);

        // Pressed -> ...
        auto t_pressed_nothing = new MoveOnNothing_Transition{*this};
        t_pressed_nothing->setTargetState(movingOnNothingState);
        pressedState->addTransition(t_pressed_nothing);

        /// MoveOnNothing -> ...
        // MoveOnNothing -> MoveOnNothing. Nothing particular to trigger.
        auto t_move_nothing_nothing = new MoveOnNothing_Transition{*this};
        t_move_nothing_nothing->setTargetState(movingOnNothingState);
        movingOnNothingState->addTransition(t_move_nothing_nothing);

        // MoveOnNothing -> MoveOnEvent.
        auto t_move_nothing_event = new MoveOnEvent_Transition{*this};
        t_move_nothing_event->setTargetState(movingOnEventState);
        movingOnNothingState->addTransition(t_move_nothing_event);

        connect(t_move_nothing_event, &MoveOnEvent_Transition::triggered,
                [&] () {  m_dispatcher.rollback(); createConstraintBetweenEvents(); });

        // MoveOnNothing -> MoveOnTimeNode
        auto t_move_nothing_timenode = new MoveOnTimeNode_Transition{*this};
        t_move_nothing_timenode->setTargetState(movingOnTimeNodeState);
        movingOnNothingState->addTransition(t_move_nothing_timenode);

        connect(t_move_nothing_timenode, &MoveOnEvent_Transition::triggered,
                [&] () {  m_dispatcher.rollback(); createEventFromEventOnTimeNode(); });



        /// MoveOnEvent -> ...
        // MoveOnEvent -> MoveOnNothing
        auto t_move_event_nothing = new MoveOnNothing_Transition{*this};
        t_move_event_nothing->setTargetState(movingOnNothingState);
        movingOnEventState->addTransition(t_move_event_nothing);

        connect(t_move_event_nothing, &MoveOnEvent_Transition::triggered,
                [&] () {  m_dispatcher.rollback();  createEventFromEventOnNothing(); });

        // No need to do anything when staying on event.

        // MoveOnEvent -> MoveOnTimeNode
        auto t_move_event_timenode = new MoveOnTimeNode_Transition{*this};
        t_move_event_timenode->setTargetState(movingOnTimeNodeState);
        movingOnEventState->addTransition(t_move_event_timenode);

        connect(t_move_event_timenode, &MoveOnEvent_Transition::triggered,
                [&] () {  m_dispatcher.rollback(); createEventFromEventOnTimeNode(); });



        /// MoveOnTimeNode -> ...
        // MoveOnTimeNode -> MoveOnNothing
        auto t_move_timenode_nothing = new MoveOnNothing_Transition{*this};
        t_move_timenode_nothing->setTargetState(movingOnNothingState);
        movingOnTimeNodeState->addTransition(t_move_timenode_nothing);

        connect(t_move_timenode_nothing, &MoveOnEvent_Transition::triggered,
                [&] () {  m_dispatcher.rollback(); createEventFromEventOnNothing(); });


        // MoveOnTimeNode -> MoveOnEvent
        auto t_move_timenode_event = new MoveOnEvent_Transition{*this};
        t_move_timenode_event->setTargetState(movingOnEventState);
        movingOnTimeNodeState->addTransition(t_move_timenode_event);

        connect(t_move_timenode_event, &MoveOnEvent_Transition::triggered,
                [&] () {  m_dispatcher.rollback(); createConstraintBetweenEvents(); });


        // MoveOnTimeNode -> MoveOnTimeNode
        auto t_move_timenode_timenode = new MoveOnTimeNode_Transition{*this};
        t_move_timenode_timenode->setTargetState(movingOnTimeNodeState);
        movingOnTimeNodeState->addTransition(t_move_timenode_timenode);

        // What happens in each state.
        QObject::connect(pressedState, &QState::entered,
                         this, &CreateEventState::createEventFromEventOnNothing);

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
    // TODO use event instead.
    // mainState->addTransition(this, SIGNAL(cancel()), rollbackState);
    rollbackState->addTransition(finalState);
    QObject::connect(rollbackState, &QState::entered, [&] ()
    {
        m_dispatcher.rollback();
    });

    setInitialState(mainState);
}


// Note : clickedEvent is set at startEvent if clicking in the background.
void CreateEventState::createEventFromEventOnNothing()
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

void CreateEventState::createEventFromEventOnTimeNode()
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


void CreateEventState::createEventFromTimeNodeOnNothing()
{
    // TODO Faire CreateEventAfterTimeNode
    // Macro : CreateEventOnTimeNode, followed by CreateEventAfterEvent[OnTimeNode], followed by Move...;
    // Maybe use a set of ObjectIdentifier for the createdEvents / timenodes ??
}

void CreateEventState::createEventFromTimeNodeOnTimeNode()
{
    // TODO
}


void CreateEventState::createConstraintBetweenEvents()
{
    auto cmd = new Scenario::Command::CreateConstraint(
                   ObjectPath{m_scenarioPath},
                   clickedEvent,
                   hoveredEvent);

    m_dispatcher.submitCommand(cmd);
}

