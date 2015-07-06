#include "ScenarioCreation_FromState.hpp"
#include "Document/TimeNode/TimeNodeModel.hpp"
#include "Process/Temporal/StateMachines/ScenarioStateMachine.hpp"

#include "Commands/Scenario/Displacement/MoveEvent.hpp"
#include "Commands/Scenario/Displacement/MoveNewEvent.hpp"
#include "Commands/Scenario/Displacement/MoveNewState.hpp"

#include "Commands/Scenario/Creations/CreateConstraint.hpp"
#include "Commands/TimeNode/MergeTimeNodes.hpp"

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
    ScenarioCreationState{stack, std::move(scenarioPath), parent},
    m_scenarioSM{stateMachine}
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
        makeTransition(pressed, move_state, [&] () { createToNothing(); });
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
        });

        QObject::connect(move_nothing, &QState::entered, [&] ()
        {
            if(!createdConstraints.empty() && !createdEvents.empty())
            {
                if(!m_scenarioSM.isShiftPressed())
                {
                    const auto&  st = m_scenarioSM.model().state(clickedState);
                    currentPoint.y = st.heightPercentage();
                }

                m_dispatcher.submitCommand<MoveNewEvent>(
                            ObjectPath{m_scenarioPath},
                            createdConstraints.last(), // TODO CheckMe
                            createdEvents.last(),// TODO CheckMe
                            currentPoint.date,
                            currentPoint.y,
                            !stateMachine.isShiftPressed());
            }
        });

        QObject::connect(move_timenode, &QState::entered, [&] ()
        {
            if(!createdEvents.empty())
            {
                m_dispatcher.submitCommand<MoveNewState>(
                            ObjectPath{m_scenarioPath},
                            createdEvents.last(),// TODO CheckMe
                            currentPoint.y);
            }
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
    creationCheck([&] (const id_type<StateModel>& id) { createToNothing_base(id); });
    /*
    const auto& scenar = m_scenarioSM.model();
    if(m_scenarioSM.isShiftPressed())
    {
        // Create new state
        auto cmd = new CreateState{m_scenarioPath, scenar.state(clickedState).eventId(), currentPoint.y};
        m_dispatcher.submitCommand(cmd);

        createdStates.append(cmd->createdState());
        createToNothing_base(createdStates.first());
    }
    else
    {
        const auto& st = scenar.state(clickedState);
        // TODO in move, if! shiftpressed, currentPoint = clickedState.y
        if(!st.nextConstraint()) // TODO & deltaX > deltaX
        {
            currentPoint.y = st.heightPercentage();
            createToNothing_base(clickedState);
        }
        else
        {
            // create a single state on the same event.
        }
    }
    */
}

void ScenarioCreation_FromState::createToTimeNode()
{
    creationCheck([&] (const id_type<StateModel>& id) { createToTimeNode_base(id); });
}

void ScenarioCreation_FromState::createToEvent()
{
    creationCheck([&] (const id_type<StateModel>& id) { createToEvent_base(id); });
}

void ScenarioCreation_FromState::createToState()
{
    creationCheck([&] (const id_type<StateModel>& id) { createToState_base(id); });
}


