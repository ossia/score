#include "ScenarioCreation_FromTimeNode.hpp"

#include "Document/TimeNode/TimeNodeModel.hpp"
#include "Process/Temporal/StateMachines/ScenarioStateMachine.hpp"
#include <Process/ScenarioModel.hpp>

#include "Commands/Scenario/Displacement/MoveEvent.hpp"
#include "Commands/Scenario/Displacement/MoveEvent2.hpp"
#include "Commands/Scenario/Displacement/MoveNewEvent.hpp"

#include "Commands/Scenario/Creations/CreateConstraint.hpp"
#include "Commands/TimeNode/MergeTimeNodes.hpp"

#include "Process/Temporal/StateMachines/Transitions/NothingTransitions.hpp"
#include "Process/Temporal/StateMachines/Transitions/AnythingTransitions.hpp"
#include "Process/Temporal/StateMachines/Transitions/EventTransitions.hpp"
#include "Process/Temporal/StateMachines/Transitions/StateTransitions.hpp"
#include "Process/Temporal/StateMachines/Transitions/ConstraintTransitions.hpp"
#include "Process/Temporal/StateMachines/Transitions/TimeNodeTransitions.hpp"

#include <QFinalState>

#include "../ScenarioRollbackStrategy.hpp"

using namespace Scenario::Command;

ScenarioCreation_FromTimeNode::ScenarioCreation_FromTimeNode(
        const ScenarioStateMachine& stateMachine,
        const Path<ScenarioModel>& scenarioPath,
        iscore::CommandStack& stack,
        QState* parent):
    ScenarioCreationState{stateMachine, stack, std::move(scenarioPath), parent}
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
        auto t_pressed_moving_nothing =
                make_transition<MoveOnNothing_Transition>(
                    pressed, move_nothing, *this);

        connect(t_pressed_moving_nothing, &QAbstractTransition::triggered,
                [&] ()
        {
            rollback();
            createToNothing();
        });


        /// MoveOnNothing -> ...
        // MoveOnNothing -> MoveOnNothing.
        make_transition<MoveOnNothing_Transition>(move_nothing, move_nothing, *this);

        // MoveOnNothing -> MoveOnState.
        add_transition(move_nothing, move_state,
                       [&] () { rollback(); ISCORE_TODO; });

        // MoveOnNothing -> MoveOnEvent.
        add_transition(move_nothing, move_event,
                       [&] () {
            if(createdEvents.contains(hoveredEvent))
            {
                return;
            }
            rollback();

            createToEvent();
        });

        // MoveOnNothing -> MoveOnTimeNode
        add_transition(move_nothing, move_timenode,
                       [&] () {
            if(createdTimeNodes.contains(hoveredTimeNode))
            {
                return;
            }
            rollback();
            createToTimeNode();
        });


        /// MoveOnState -> ...
        // MoveOnState -> MoveOnNothing
        add_transition(move_state, move_nothing,
                       [&] () { rollback(); ISCORE_TODO; });

        // MoveOnState -> MoveOnState
        // We don't do anything, the constraint should not move.

        // MoveOnState -> MoveOnEvent
        add_transition(move_state, move_event,
                       [&] () { rollback(); ISCORE_TODO; });

        // MoveOnState -> MoveOnTimeNode
        add_transition(move_state, move_timenode,
                       [&] () { rollback(); ISCORE_TODO; });


        /// MoveOnEvent -> ...
        // MoveOnEvent -> MoveOnNothing
        add_transition(move_event, move_nothing,
                       [&] () {
            rollback();
            createToNothing();
        });

        // MoveOnEvent -> MoveOnState
        add_transition(move_event, move_state,
                       [&] () { rollback(); ISCORE_TODO; });

        // MoveOnEvent -> MoveOnEvent
        make_transition<MoveOnEvent_Transition>(move_event, move_event, *this);

        // MoveOnEvent -> MoveOnTimeNode
        add_transition(move_event, move_timenode,
                       [&] () {
            if(createdTimeNodes.contains(hoveredTimeNode))
            {
                return;
            }
            rollback();
            createToTimeNode();
        });


        /// MoveOnTimeNode -> ...
        // MoveOnTimeNode -> MoveOnNothing
        add_transition(move_timenode, move_nothing,
                       [&] () {
            rollback();
            createToNothing();
        });

        // MoveOnTimeNode -> MoveOnState
        add_transition(move_timenode, move_state,
                       [&] () { rollback(); ISCORE_TODO; });

        // MoveOnTimeNode -> MoveOnEvent
        add_transition(move_timenode, move_event,
                       [&] () {
            if(createdEvents.contains(hoveredEvent))
            {
                rollback();
                return;
            }
            rollback();
            createToEvent();
        });

        // MoveOnTimeNode -> MoveOnTimeNode
        make_transition<MoveOnTimeNode_Transition>(move_timenode , move_timenode , *this);



        // What happens in each state.
        QObject::connect(pressed, &QState::entered,
                         [&] ()
        {
            m_clickedPoint = currentPoint;
            createInitialEventAndState();
        });

        QObject::connect(move_nothing, &QState::entered, [&] ()
        {
            if(createdEvents.empty() || createdConstraints.empty())
            {
                rollback();
                return;
            }
            // Move the timenode
            m_dispatcher.submitCommand<MoveNewEvent>(
                        Path<ScenarioModel>{m_scenarioPath},
                        createdConstraints.last(),
                        createdEvents.last(),
                        currentPoint.date,
                        currentPoint.y,
                        !stateMachine.isShiftPressed());
        });

        QObject::connect(move_timenode, &QState::entered, [&] ()
        {
            if(createdEvents.empty())
            {
                rollback();
                return;
            }

            m_dispatcher.submitCommand<MoveEvent2<GoodOldDisplacementPolicy>>(
                        Path<ScenarioModel>{m_scenarioPath},
                        createdEvents.last(),
                        TimeValue::zero(),
                        stateMachine.expandMode());
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
void ScenarioCreation_FromTimeNode::createInitialEventAndState()
{
    auto cmd = new CreateEvent_State{m_scenarioPath, clickedTimeNode, currentPoint.y};
    m_dispatcher.submitCommand(cmd);

    createdStates.append(cmd->createdState());
    createdEvents.append(cmd->createdEvent());
}

void ScenarioCreation_FromTimeNode::createToNothing()
{
    createInitialEventAndState();
    createToNothing_base(createdStates.first());
}

void ScenarioCreation_FromTimeNode::createToState()
{
    createInitialEventAndState();
    createToState_base(createdStates.first());
}

void ScenarioCreation_FromTimeNode::createToTimeNode()
{
    // TODO "if hoveredTimeNode != clickedTimeNode"
    createInitialEventAndState();
    createToTimeNode_base(createdStates.first());
}

void ScenarioCreation_FromTimeNode::createToEvent()
{
    createInitialEventAndState();
    createToEvent_base(createdStates.first());
}

