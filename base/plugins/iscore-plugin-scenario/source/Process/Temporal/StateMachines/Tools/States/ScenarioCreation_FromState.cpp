#include "ScenarioCreation_FromState.hpp"
#include "Document/TimeNode/TimeNodeModel.hpp"
#include "Process/Temporal/StateMachines/ScenarioStateMachine.hpp"
#include <Process/ScenarioModel.hpp>

#include "Commands/Scenario/Displacement/MoveEvent.hpp"
#include "Commands/Scenario/Displacement/MoveNewEvent.hpp"
#include "Commands/Scenario/Displacement/MoveNewState.hpp"
#include "Commands/Scenario/Creations/CreateEventAfterEvent.hpp"
#include "Commands/Scenario/Creations/CreateEventAfterEventOnTimeNode.hpp"
#include "Commands/Scenario/Creations/CreateConstraint.hpp"
#include "Commands/TimeNode/MergeTimeNodes.hpp"
#include "Commands/Scenario/Creations/CreateEventOnTimeNode.hpp"

#include "Process/Temporal/StateMachines/Transitions/NothingTransitions.hpp"
#include "Process/Temporal/StateMachines/Transitions/AnythingTransitions.hpp"
#include "Process/Temporal/StateMachines/Transitions/EventTransitions.hpp"
#include "Process/Temporal/StateMachines/Transitions/ConstraintTransitions.hpp"
#include "Process/Temporal/StateMachines/Transitions/TimeNodeTransitions.hpp"
#include "Process/Temporal/StateMachines/Transitions/StateTransitions.hpp"

#include "../ScenarioRollbackStrategy.hpp"
#include <QFinalState>

using namespace Scenario::Command;

ScenarioCreation_FromState::ScenarioCreation_FromState(
        const ScenarioStateMachine& stateMachine,
        ObjectPath &&scenarioPath,
        iscore::CommandStack& stack,
        QState* parent):
    ScenarioCreationState{stack, std::move(scenarioPath), parent}
{
    using namespace Scenario::Command;
    auto finalState = new QFinalState{this};
    connect(finalState, &QState::entered, [&] ()
    {
        clearCreatedIds();
    });

    QState* mainState = new QState{this};
    {
        auto pressed = new QState{mainState};
        auto released = new QState{mainState};
        auto move_nothing = new StrongQState<ScenarioElement::Nothing + Modifier::Move_tag::value>{mainState};
        auto move_state = new StrongQState<ScenarioElement::State + Modifier::Move_tag::value>{mainState};
        auto move_event = new StrongQState<ScenarioElement::Event + Modifier::Move_tag::value>{mainState};
        auto move_timenode = new StrongQState<ScenarioElement::TimeNode + Modifier::Move_tag::value>{mainState};

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
                       [&] () { rollback(); createToState(); });

        // MoveOnNothing -> MoveOnEvent.
        makeTransition(move_nothing, move_event,
                       [&] () { rollback(); createToEvent(); });

        // MoveOnNothing -> MoveOnTimeNode
        makeTransition(move_nothing, move_timenode,
                       [&] () { rollback(); createToTimeNode(); });


        /// MoveOnState -> ...
        // MoveOnState -> MoveOnNothing
        makeTransition(move_state, move_nothing,
                       [&] () { rollback(); createToNothing(); });

        // MoveOnState -> MoveOnState
        // We don't do anything, the constraint should not move.

        // MoveOnState -> MoveOnEvent
        makeTransition(move_state, move_event,
                       [&] () { rollback(); createToEvent(); });

        // MoveOnState -> MoveOnTimeNode
        makeTransition(move_state, move_timenode,
                       [&] () { rollback(); createToTimeNode(); });


        /// MoveOnEvent -> ...
        // MoveOnEvent -> MoveOnNothing
        makeTransition(move_event, move_nothing,
                       [&] () { rollback(); createToNothing(); });

        // MoveOnEvent -> MoveOnState
        makeTransition(move_event, move_state,
                       [&] () { rollback(); createToState(); });

        // MoveOnEvent -> MoveOnEvent
        make_transition<MoveOnEvent_Transition>(move_event, move_event, *this);

        // MoveOnEvent -> MoveOnTimeNode
        makeTransition(move_event, move_timenode,
                       [&] () { rollback(); createToTimeNode(); });


        /// MoveOnTimeNode -> ...
        // MoveOnTimeNode -> MoveOnNothing
        makeTransition(move_timenode, move_nothing,
                       [&] () { rollback(); createToNothing(); });

        // MoveOnTimeNode -> MoveOnState
        makeTransition(move_timenode, move_state,
                       [&] () { rollback(); createToState(); });

        // MoveOnTimeNode -> MoveOnEvent
        makeTransition(move_timenode, move_event,
                       [&] () { rollback(); createToEvent(); });

        // MoveOnTimeNode -> MoveOnTimeNode
        make_transition<MoveOnTimeNode_Transition>(move_timenode , move_timenode , *this);



        // What happens in each state.
        QObject::connect(pressed, &QState::entered,
                         [&] ()
        {
            m_clickedPoint = currentPoint;
            createToNothing();
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

        QObject::connect(move_timenode, &QState::entered, [&] ()
        {
            // TODO why ?
            m_dispatcher.submitCommand<MoveNewState>(
                        ObjectPath{m_scenarioPath},
                        createdEvents.last(),// TODO CheckMe
                        currentPoint.y);
        });

        QObject::connect(released, &QState::entered, [&] ()
        {
            m_dispatcher.commit<Scenario::Command::CreationMetaCommand>();
        });
    }

    QState* rollbackState = new QState{this};
    make_transition<Cancel_Transition>(mainState, rollbackState);
    rollbackState->addTransition(finalState);
    QObject::connect(rollbackState, &QState::entered, [&] ()
    {
        rollback();
    });

    setInitialState(mainState);
}


// Note : clickedEvent is set at startEvent if clicking in the background.
void ScenarioCreation_FromState::createToNothing()
{
    createToNothing_base(clickedState);
}

void ScenarioCreation_FromState::createToTimeNode()
{
    createToTimeNode_base(clickedState);
}

void ScenarioCreation_FromState::createToEvent()
{
    createToEvent_base(clickedState);
}

void ScenarioCreation_FromState::createToState()
{
    createToState_base(clickedState);
}
