#include "CreateEventState.hpp"
#include "Document/TimeNode/TimeNodeModel.hpp"
#include "Process/Temporal/StateMachines/ScenarioStateMachine.hpp"
#include <Process/ScenarioModel.hpp>

#include "Commands/Scenario/Displacement/MoveEvent.hpp"
#include "Commands/Scenario/Displacement/MoveEventAndConstraint.hpp"
#include "Commands/Scenario/Creations/CreateEventAfterEvent.hpp"
#include "Commands/Scenario/Creations/CreateEventAfterEventOnTimeNode.hpp"
#include "Commands/Scenario/Creations/CreateConstraint.hpp"
#include "Commands/TimeNode/MergeTimeNodes.hpp"

#include "Process/Temporal/StateMachines/Transitions/NothingTransitions.hpp"
#include "Process/Temporal/StateMachines/Transitions/AnythingTransitions.hpp"
#include "Process/Temporal/StateMachines/Transitions/EventTransitions.hpp"
#include "Process/Temporal/StateMachines/Transitions/ConstraintTransitions.hpp"
#include "Process/Temporal/StateMachines/Transitions/TimeNodeTransitions.hpp"

#include <QFinalState>
using namespace Scenario::Command;
CreateFromEventState::CreateFromEventState(
        const ScenarioStateMachine& stateMachine,
        ObjectPath &&scenarioPath,
        iscore::CommandStack& stack,
        QState* parent):
    CreationState{std::move(scenarioPath), parent},
    m_dispatcher{stack}
{
    using namespace Scenario::Command;
    auto finalState = new QFinalState{this};
    connect(finalState, &QState::entered, [&] ()
    {
        setCreatedEvent(id_type<EventModel>{});
        setCreatedTimeNode(id_type<TimeNodeModel>{});
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
                         this, &CreateFromEventState::createEventFromEventOnNothing);

        QObject::connect(movingOnNothingState, &QState::entered, [&] ()
        {
            m_dispatcher.submitCommand<MoveEventAndConstraint>(
                            ObjectPath{m_scenarioPath},
                            createdConstraint(),
                            createdEvent(),
                            currentPoint.date,
                            currentPoint.y,
                            stateMachine.expandMode());
        });

        QObject::connect(movingOnTimeNodeState, &QState::entered, [&] ()
        {
            m_dispatcher.submitCommand<MoveEventAndConstraint>(
                            ObjectPath{m_scenarioPath},
                            createdConstraint(),
                            createdEvent(),
                            m_scenarioPath.find<ScenarioModel>().timeNode(hoveredTimeNode).date(),
                            currentPoint.y,
                            stateMachine.expandMode());
        });

        QObject::connect(releasedState, &QState::entered, [&] ()
        {
            m_dispatcher.commit<Scenario::Command::CreationMetaCommand>();
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
void CreateFromEventState::createEventFromEventOnNothing()
{
    auto cmd = new CreateEventAfterEvent{
                ObjectPath{m_scenarioPath},
                clickedEvent,
                currentPoint.date,
                currentPoint.y};
    setCreatedEvent(cmd->createdEvent());
    setCreatedTimeNode(cmd->createdTimeNode());
    setCreatedConstraint(cmd->createdConstraint());

    m_dispatcher.submitCommand(cmd);
}

void CreateFromEventState::createEventFromEventOnTimeNode()
{
    auto cmd = new CreateEventAfterEventOnTimeNode(
                   ObjectPath{m_scenarioPath},
                   clickedEvent,
                   hoveredTimeNode,
                   currentPoint.date,
                   currentPoint.y);

    setCreatedEvent(cmd->createdEvent());
    setCreatedTimeNode(id_type<TimeNodeModel>{});
    setCreatedConstraint(cmd->createdConstraint());

    m_dispatcher.submitCommand(cmd);
}

void CreateFromEventState::createConstraintBetweenEvents()
{
    auto cmd = new CreateConstraint{
              ObjectPath{m_scenarioPath},
              clickedEvent,
              hoveredEvent};

    setCreatedConstraint(cmd->createdConstraint());

    m_dispatcher.submitCommand(cmd);
}



CreateFromTimeNodeState::CreateFromTimeNodeState(
        const ScenarioStateMachine& stateMachine,
        ObjectPath &&scenarioPath,
        iscore::CommandStack& stack,
        QState* parent):
    CreationState{std::move(scenarioPath), parent},
    m_dispatcher{stack}
{
    using namespace Scenario::Command;
    auto finalState = new QFinalState{this};
    connect(finalState, &QState::entered, [&] ()
    {
        setCreatedEvent(id_type<EventModel>{});
        setCreatedTimeNode(id_type<TimeNodeModel>{});
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
        auto t_pressed_moving_nothing =
                make_transition<MoveOnNothing_Transition>(
                    pressedState, movingOnNothingState, *this);

        connect(t_pressed_moving_nothing, &QAbstractTransition::triggered,
                [&] ()
        {
            createEventFromEventOnNothing();
        });

        /// MoveOnNothing -> ...
        // MoveOnNothing -> MoveOnNothing. Nothing particular to trigger.
        make_transition<MoveOnNothing_Transition>(
                    movingOnNothingState, movingOnNothingState, *this);


        // MoveOnNothing -> MoveOnEvent.
        auto t_move_nothing_event =
                make_transition<MoveOnEvent_Transition>(
                    movingOnNothingState, movingOnEventState, *this);

        connect(t_move_nothing_event, &QAbstractTransition::triggered,
                [&] ()
        {
            if(hoveredEvent == m_createdFirstEvent || hoveredEvent == createdEvent())
            {
                return;
            }
            m_dispatcher.rollback();
            createSingleEventOnTimeNode();
            createConstraintBetweenEvents();
        });

        // MoveOnNothing -> MoveOnTimeNode
        auto t_move_nothing_timenode =
                make_transition<MoveOnTimeNode_Transition>(
                    movingOnNothingState, movingOnTimeNodeState, *this);

        connect(t_move_nothing_timenode, &QAbstractTransition::triggered,
                [&] ()
        {
            if(hoveredTimeNode == createdTimeNode())
            {
                return;
            }
            m_dispatcher.rollback();
            createSingleEventOnTimeNode();
            createEventFromEventOnTimenode();
         });


        /// MoveOnEvent -> ...
        // MoveOnEvent -> MoveOnNothing
        auto t_move_event_nothing =
                make_transition<MoveOnNothing_Transition>(
                    movingOnEventState, movingOnNothingState, *this);

        connect(t_move_event_nothing, &QAbstractTransition::triggered,
                [&] ()
        {
            m_dispatcher.rollback();
            createSingleEventOnTimeNode();
            createEventFromEventOnNothing();
         });

        // MoveOnEvent -> MoveOnEvent : nothing to do.

        // MoveOnEvent -> MoveOnTimeNode
        auto t_move_event_timenode =
                make_transition<MoveOnTimeNode_Transition>(
                    movingOnEventState, movingOnTimeNodeState, *this);

        connect(t_move_event_timenode, &QAbstractTransition::triggered,
                [&] ()
        {
            if(hoveredTimeNode == createdTimeNode())
            {
                return;
            }
            m_dispatcher.rollback();
            createSingleEventOnTimeNode();
            createEventFromEventOnTimenode();
        });


        /// MoveOnTimeNode -> ...
        // MoveOnTimeNode -> MoveOnNothing
        auto t_move_timenode_nothing =
                make_transition<MoveOnNothing_Transition>(
                    movingOnTimeNodeState, movingOnNothingState, *this);

        connect(t_move_timenode_nothing, &QAbstractTransition::triggered,
                [&] ()
        {
            m_dispatcher.rollback();
            createSingleEventOnTimeNode();
            createEventFromEventOnNothing();
         });


        // MoveOnTimeNode -> MoveOnEvent
        auto t_move_timenode_event =
                make_transition<MoveOnEvent_Transition>(
                    movingOnTimeNodeState, movingOnEventState, *this);

        connect(t_move_timenode_event, &QAbstractTransition::triggered,
                [&] ()
        {
            if(hoveredEvent == m_createdFirstEvent || hoveredEvent == createdEvent())
            {
                return;
            }
            m_dispatcher.rollback();
            createSingleEventOnTimeNode();
            createConstraintBetweenEvents();
        });


        // MoveOnTimeNode -> MoveOnTimeNode
        make_transition<MoveOnTimeNode_Transition>(
                    movingOnTimeNodeState, movingOnTimeNodeState, *this);

        // What happens in each state.
        QObject::connect(pressedState, &QState::entered,
                         [&] ()
        {
            m_clickedPoint = currentPoint;
            createSingleEventOnTimeNode();
        });

        QObject::connect(movingOnNothingState, &QState::entered, [&] ()
        {
            m_dispatcher.submitCommand<MoveEvent>(
                            ObjectPath{m_scenarioPath},
                            createdEvent(),
                            currentPoint.date,
                            currentPoint.y,
                            stateMachine.expandMode());
        });

        QObject::connect(movingOnTimeNodeState, &QState::entered, [&] ()
        {
            m_dispatcher.submitCommand<MoveEvent>(
                            ObjectPath{m_scenarioPath},
                            createdEvent(),
                            m_scenarioPath.find<ScenarioModel>().timeNode(hoveredTimeNode).date(),
                            currentPoint.y,
                            stateMachine.expandMode());
        });

        QObject::connect(releasedState, &QState::entered, [&] ()
        {
            m_dispatcher.commit<Scenario::Command::CreationMetaCommand>();
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
#include "Commands/Scenario/Creations/CreateEventOnTimeNode.hpp"
void CreateFromTimeNodeState::createSingleEventOnTimeNode()
{
    auto cmd =
            new CreateEventOnTimeNode(
                ObjectPath{m_scenarioPath},
                clickedTimeNode,
                m_clickedPoint.y);
    m_dispatcher.submitCommand(cmd);

    m_createdFirstEvent = cmd->createdEventId();
}

void CreateFromTimeNodeState::createEventFromEventOnNothing()
{
    // If we start moving, we have to put a createEventAfterEvent.
    // Note : there will be two created events; how to prevent a mess in collision management ?
    auto cmd = new CreateEventAfterEvent{
               ObjectPath{m_scenarioPath},
               m_createdFirstEvent,
               currentPoint.date,
               currentPoint.y};

    setCreatedEvent(cmd->createdEvent());
    setCreatedTimeNode(cmd->createdTimeNode());
    m_dispatcher.submitCommand(cmd);
}

void CreateFromTimeNodeState::createEventFromEventOnTimenode()
{
    auto cmd = new CreateEventAfterEventOnTimeNode(
                   ObjectPath{m_scenarioPath},
                   m_createdFirstEvent,
                   hoveredTimeNode,
                   currentPoint.date,
                   currentPoint.y);

    setCreatedEvent(cmd->createdEvent());
    setCreatedTimeNode(id_type<TimeNodeModel>{});
    m_dispatcher.submitCommand(cmd);
}

void CreateFromTimeNodeState::createConstraintBetweenEvents()
{
    auto cmd = new CreateConstraint(
                   ObjectPath{m_scenarioPath},
                   m_createdFirstEvent,
                   hoveredEvent);

    m_dispatcher.submitCommand(cmd);
}
