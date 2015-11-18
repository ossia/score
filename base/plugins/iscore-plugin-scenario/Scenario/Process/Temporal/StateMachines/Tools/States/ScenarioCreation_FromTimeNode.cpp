#include "ScenarioCreation_FromTimeNode.hpp"

#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Process/Temporal/StateMachines/ScenarioStateMachine.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <Scenario/Commands/Scenario/Displacement/MoveEventMeta.hpp>
#include <Scenario/Commands/Scenario/Displacement/MoveNewEvent.hpp>

#include <Scenario/Commands/Scenario/Creations/CreateConstraint.hpp>
#include <Scenario/Commands/TimeNode/MergeTimeNodes.hpp>

#include <Scenario/Process/Temporal/StateMachines/Transitions/NothingTransitions.hpp>
#include <Scenario/Process/Temporal/StateMachines/Transitions/AnythingTransitions.hpp>
#include <Scenario/Process/Temporal/StateMachines/Transitions/EventTransitions.hpp>
#include <Scenario/Process/Temporal/StateMachines/Transitions/StateTransitions.hpp>
#include <Scenario/Process/Temporal/StateMachines/Transitions/ConstraintTransitions.hpp>
#include <Scenario/Process/Temporal/StateMachines/Transitions/TimeNodeTransitions.hpp>

#include <Scenario/Process/Temporal/StateMachines/Tools/ScenarioRollbackStrategy.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioPresenter.hpp>
#include <QFinalState>

using namespace Scenario::Command;
namespace Scenario
{
Creation_FromTimeNode::Creation_FromTimeNode(
        const ToolPalette& stateMachine,
        const Path<ScenarioModel>& scenarioPath,
        iscore::CommandStack& stack,
        QState* parent):
    CreationState{stateMachine, stack, std::move(scenarioPath), parent}
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
                        stateMachine.editionSettings().sequence());
        });

        QObject::connect(move_timenode, &QState::entered, [&] ()
        {
            if(createdEvents.empty())
            {
                rollback();
                return;
            }

            m_dispatcher.submitCommand<MoveEventMeta>(
                        Path<ScenarioModel>{m_scenarioPath},
                        createdEvents.last(),
                        TimeValue::zero(),
                        stateMachine.editionSettings().expandMode());
        });

        QObject::connect(released, &QState::entered, [&] ()
        {
            this->makeSnapshot();
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
void Creation_FromTimeNode::createInitialEventAndState()
{
    auto cmd = new CreateEvent_State{m_scenarioPath, clickedTimeNode, currentPoint.y};
    m_dispatcher.submitCommand(cmd);

    createdStates.append(cmd->createdState());
    createdEvents.append(cmd->createdEvent());
}

void Creation_FromTimeNode::createToNothing()
{
    createInitialEventAndState();
    createToNothing_base(createdStates.first());
}

void Creation_FromTimeNode::createToState()
{
    createInitialEventAndState();
    createToState_base(createdStates.first());
}

void Creation_FromTimeNode::createToTimeNode()
{
    // TODO "if hoveredTimeNode != clickedTimeNode"
    createInitialEventAndState();
    createToTimeNode_base(createdStates.first());
}

void Creation_FromTimeNode::createToEvent()
{
    createInitialEventAndState();
    createToEvent_base(createdStates.first());
}

}
