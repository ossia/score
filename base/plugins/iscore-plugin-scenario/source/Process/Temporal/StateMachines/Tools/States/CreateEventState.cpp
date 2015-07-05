#include "CreateEventState.hpp"
#include "Document/TimeNode/TimeNodeModel.hpp"
#include "Process/Temporal/StateMachines/ScenarioStateMachine.hpp"
#include <Process/ScenarioModel.hpp>

#include "Commands/Scenario/Displacement/MoveEvent.hpp"
#include "Commands/Scenario/Displacement/MoveNewEvent.hpp"
#include "Commands/Scenario/Displacement/MoveNewState.hpp"
#include "Commands/Scenario/Displacement/MoveTimeNode.hpp"


#include "Commands/Scenario/Creations/CreateConstraint.hpp"
#include "Commands/Scenario/Creations/CreateConstraint_State.hpp"
#include "Commands/Scenario/Creations/CreateConstraint_State_Event.hpp"
#include "Commands/Scenario/Creations/CreateConstraint_State_Event_TimeNode.hpp"
#include "Commands/Scenario/Creations/CreateState.hpp"
#include "Commands/Scenario/Creations/CreateEvent_State.hpp"

#include "Commands/Scenario/Creations/CreateEventAfterEvent.hpp"
#include "Commands/Scenario/Creations/CreateEventAfterEventOnTimeNode.hpp"
#include "Commands/Scenario/Creations/CreateEventOnTimeNode.hpp"

#include "Commands/TimeNode/MergeTimeNodes.hpp"

#include "Process/Temporal/StateMachines/Transitions/NothingTransitions.hpp"
#include "Process/Temporal/StateMachines/Transitions/AnythingTransitions.hpp"
#include "Process/Temporal/StateMachines/Transitions/StateTransitions.hpp"
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
        clearCreatedIds();
    });

    QState* mainState = new QState{this};
    mainState->setObjectName("Main state");
    {
        auto pressed = new QState{mainState};
        auto released = new QState{mainState};
        auto move_nothing = new StrongQState<ScenarioElement::Nothing + Modifier::Move_tag::value>{mainState};
        auto move_state = new StrongQState<ScenarioElement::State + Modifier::Move_tag::value>{mainState};
        auto move_event = new StrongQState<ScenarioElement::Event + Modifier::Move_tag::value>{mainState};
        auto move_timenode = new StrongQState<ScenarioElement::TimeNode + Modifier::Move_tag::value>{mainState};

        pressed->setObjectName("Pressed");
        released->setObjectName("Released");
        move_nothing->setObjectName("Move on Nothing");
        move_state->setObjectName("Move on State");
        move_event->setObjectName("Move on Event");
        move_timenode->setObjectName("Move on TimeNode");
        // General setup
        mainState->setInitialState(pressed);
        released->addTransition(finalState);

        // Release
        make_transition<ReleaseOnAnything_Transition>(mainState, released);

        // Pressed -> ...
        make_transition<MoveOnNothing_Transition>(pressed, move_nothing, *this);

        /// MoveOnNothing -> ...
        // MoveOnNothing -> MoveOnNothing.
        make_transition<MoveOnNothing_Transition>(move_nothing, move_nothing, *this);

        // MoveOnNothing -> MoveOnState.
        makeTransition(move_nothing, move_state,
                       [&] () { rollback(); createConstraintBetweenEventAndState(); });

        // MoveOnNothing -> MoveOnEvent.
        makeTransition(move_nothing, move_event,
                       [&] () { rollback(); createConstraintBetweenEvents(); });

        // MoveOnNothing -> MoveOnTimeNode
        makeTransition(move_nothing, move_timenode,
                       [&] () { rollback(); createConstraintBetweenEventAndTimeNode(); });


        /// MoveOnState -> ...
        // MoveOnState -> MoveOnNothing
        makeTransition(move_state, move_nothing,
                       [&] () { rollback(); createEventFromEventOnNothing(); });

        // MoveOnState -> MoveOnState
        // We don't do anything, the constraint should not move.

        // MoveOnState -> MoveOnEvent
        makeTransition(move_state, move_event,
                       [&] () { rollback(); createConstraintBetweenEvents(); });

        // MoveOnState -> MoveOnTimeNode
        makeTransition(move_state, move_timenode,
                       [&] () { rollback(); createConstraintBetweenEventAndTimeNode(); });


        /// MoveOnEvent -> ...
        // MoveOnEvent -> MoveOnNothing
        makeTransition(move_event, move_nothing,
                       [&] () { rollback(); createEventFromEventOnNothing(); });

        // MoveOnEvent -> MoveOnState
        makeTransition(move_event, move_state,
                       [&] () { rollback(); createConstraintBetweenEventAndState(); });

        // MoveOnEvent -> MoveOnEvent
        make_transition<MoveOnEvent_Transition>(move_event, move_event, *this);

        // MoveOnEvent -> MoveOnTimeNode
        makeTransition(move_event, move_timenode,
                       [&] () { rollback(); createConstraintBetweenEventAndTimeNode(); });


        /// MoveOnTimeNode -> ...
        // MoveOnTimeNode -> MoveOnNothing
        makeTransition(move_timenode, move_nothing,
                       [&] () { rollback(); createEventFromEventOnNothing(); });

        // MoveOnTimeNode -> MoveOnState
        makeTransition(move_timenode, move_state,
                       [&] () { rollback(); createConstraintBetweenEventAndState(); });

        // MoveOnTimeNode -> MoveOnEvent
        makeTransition(move_timenode, move_event,
                       [&] () { rollback(); createConstraintBetweenEvents(); });

        // MoveOnTimeNode -> MoveOnTimeNode
        make_transition<MoveOnTimeNode_Transition>(move_timenode , move_timenode , *this);

        // What happens in each state.
        QObject::connect(pressed, &QState::entered,
                         [&] ()
        {
            m_clickedPoint = currentPoint;
            createEventFromEventOnNothing();
        });

        QObject::connect(move_nothing, &QState::entered, [&] ()
        {
            // Move the timenode
            m_dispatcher.submitCommand<MoveNewEvent>(
                        ObjectPath{m_scenarioPath},
                        createdConstraints.last(), // TODO CheckMe
                        createdEvents.last(),// TODO CheckMe
                        currentPoint.date,
                        currentPoint.y,
                        !stateMachine.isShiftPressed());
        });

        QObject::connect(move_timenode , &QState::entered, [&] ()
        {
            ISCORE_TODO
                    /*
            m_dispatcher.submitCommand<MoveNewState>(
                        ObjectPath{m_scenarioPath},
                        createdEvents.last(),// TODO CheckMe
                        currentPoint.y);
                                */
        });

        QObject::connect(released, &QState::entered, [&] ()
        {
            m_dispatcher.commit<Scenario::Command::CreationMetaCommand>();
        });
    }

    QState* rollbackState = new QState{this};
    rollbackState->setObjectName("Rollback");
    make_transition<Cancel_Transition>(mainState, rollbackState);
    rollbackState->addTransition(finalState);
    QObject::connect(rollbackState, &QState::entered, [&] ()
    {
        rollback();
    });

    setInitialState(mainState);
}


// Note : clickedEvent is set at startEvent if clicking in the background.
void CreateFromEventState::createEventFromEventOnNothing()
{
    auto cmd = new CreateState{m_scenarioPath, clickedEvent, currentPoint.y};
    m_dispatcher.submitCommand(cmd);

    createdStates.append(cmd->createdState());

    auto cmd2 = new CreateConstraint_State_Event_TimeNode{
            m_scenarioPath,
            cmd->createdState(),
            currentPoint.date,
            currentPoint.y};
    m_dispatcher.submitCommand(cmd2);

    createdStates.append(cmd2->createdState());
    createdEvents.append(cmd2->createdEvent());
    createdTimeNodes.append(cmd2->createdTimeNode());
    createdConstraints.append(cmd2->createdConstraint());
}

void CreateFromEventState::createConstraintBetweenEventAndTimeNode()
{
    ISCORE_TODO
            /*
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
                    */
}

void CreateFromEventState::createConstraintBetweenEventAndState()
{

}

void CreateFromEventState::createConstraintBetweenEvents()
{
    ISCORE_TODO
    /*
    if(hoveredEvent != clickedEvent)
    {
        auto cmd = new CreateConstraint{
                  ObjectPath{m_scenarioPath},
                  clickedEvent,
                  clickedState,
                  hoveredEvent};

        setCreatedConstraint(cmd->createdConstraint());

        m_dispatcher.submitCommand(cmd);
    }
    */
}

void CreateFromEventState::createSingleEventOnTimeNode()
{
    auto& scenar = m_scenarioPath.find<ScenarioModel>();
    clickedTimeNode = scenar.event(clickedEvent).timeNode();

    auto cmd =
            new CreateEventOnTimeNode(
                ObjectPath{m_scenarioPath},
                clickedTimeNode,
                m_clickedPoint.y);
    m_dispatcher.submitCommand(cmd);
}


