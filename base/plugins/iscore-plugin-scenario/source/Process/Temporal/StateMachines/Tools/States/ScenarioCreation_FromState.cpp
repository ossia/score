#include "ScenarioCreation_FromState.hpp"
#include "Document/TimeNode/TimeNodeModel.hpp"
#include "Process/Temporal/StateMachines/ScenarioStateMachine.hpp"
#include "Process/ScenarioModel.hpp"


#include "Commands/Scenario/Creations/CreateState.hpp"
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
        add_transition(pressed, move_state, [&] () { createToNothing(); });
        make_transition<MoveOnNothing_Transition>(pressed, move_nothing, *this);

        /// MoveOnNothing -> ...
        // MoveOnNothing -> MoveOnNothing.
        make_transition<MoveOnNothing_Transition>(move_nothing, move_nothing, *this);

        // MoveOnNothing -> MoveOnState.
        add_transition(move_nothing, move_state,
                       [&] () { rollback(); createToState(); });

        // MoveOnNothing -> MoveOnEvent.
        add_transition(move_nothing, move_event,
                       [&] () { rollback(); createToEvent(); });

        // MoveOnNothing -> MoveOnTimeNode
        add_transition(move_nothing, move_timenode,
                       [&] () { rollback(); createToTimeNode(); });


        /// MoveOnState -> ...
        // MoveOnState -> MoveOnNothing
        add_transition(move_state, move_nothing,
                       [&] () { rollback(); createToNothing(); });

        // MoveOnState -> MoveOnState
        // We don't do anything, the constraint should not move.

        // MoveOnState -> MoveOnEvent
        add_transition(move_state, move_event,
                       [&] () { rollback(); createToEvent(); });

        // MoveOnState -> MoveOnTimeNode
        add_transition(move_state, move_timenode,
                       [&] () { rollback(); createToTimeNode(); });


        /// MoveOnEvent -> ...
        // MoveOnEvent -> MoveOnNothing
        add_transition(move_event, move_nothing,
                       [&] () { rollback(); createToNothing(); });

        // MoveOnEvent -> MoveOnState
        add_transition(move_event, move_state,
                       [&] () {
            if(m_parentSM.model().state(clickedState).eventId() != m_parentSM.model().state(hoveredState).eventId())
            {
                rollback();
                createToState();
            }
        });

        // MoveOnEvent -> MoveOnEvent
        make_transition<MoveOnEvent_Transition>(move_event, move_event, *this);

        // MoveOnEvent -> MoveOnTimeNode
        add_transition(move_event, move_timenode,
                       [&] () { rollback(); createToTimeNode(); });


        /// MoveOnTimeNode -> ...
        // MoveOnTimeNode -> MoveOnNothing
        add_transition(move_timenode, move_nothing,
                       [&] () { rollback(); createToNothing(); });

        // MoveOnTimeNode -> MoveOnState
        add_transition(move_timenode, move_state,
                       [&] () { rollback(); createToState(); });

        // MoveOnTimeNode -> MoveOnEvent
        add_transition(move_timenode, move_event,
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
                if(!m_parentSM.isShiftPressed())
                {
                    const auto&  st = m_parentSM.model().state(clickedState);
                    currentPoint.y = st.heightPercentage();
                }

                m_dispatcher.submitCommand<MoveNewEvent>(
                            Path<ScenarioModel>{m_scenarioPath},
                            createdConstraints.last(), // TODO CheckMe
                            createdEvents.last(),// TODO CheckMe
                            currentPoint.date,
                            currentPoint.y,
                            !stateMachine.isShiftPressed());
            }
        });

        QObject::connect(move_event, &QState::entered, [&] ()
        {
            if(!createdStates.empty())
            {
                m_dispatcher.submitCommand<MoveNewState>(
                            Path<ScenarioModel>{m_scenarioPath},
                            createdStates.last(),
                            currentPoint.y);
            }
        });

        QObject::connect(move_timenode, &QState::entered, [&] ()
        {
            if(!createdStates.empty())
            {
                m_dispatcher.submitCommand<MoveNewState>(
                            Path<ScenarioModel>{m_scenarioPath},
                            createdStates.last(),
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


template<typename Fun>
void ScenarioCreation_FromState::creationCheck(Fun&& fun)
{
    const auto& scenar = m_parentSM.model();
    if(m_parentSM.isShiftPressed())
    {
        // Create new state
        auto cmd = new Scenario::Command::CreateState{m_scenarioPath, scenar.state(clickedState).eventId(), currentPoint.y};
        m_dispatcher.submitCommand(cmd);

        createdStates.append(cmd->createdState());
        fun(createdStates.first());
    }
    else
    {
        const auto& st = scenar.state(clickedState);
        if(!st.nextConstraint()) // TODO & deltaY < deltaX
        {
            currentPoint.y = st.heightPercentage();
            fun(clickedState);
        }
        else
        {
            // create a single state on the same event (deltaY > deltaX)
        }
    }
}

// Note : clickedEvent is set at startEvent if clicking in the background.
void ScenarioCreation_FromState::createToNothing()
{
    creationCheck([&] (const Id<StateModel>& id) { createToNothing_base(id); });
}

void ScenarioCreation_FromState::createToTimeNode()
{
    creationCheck([&] (const Id<StateModel>& id) { createToTimeNode_base(id); });
}

void ScenarioCreation_FromState::createToEvent()
{
    if(hoveredEvent == m_parentSM.model().state(clickedState).eventId())
    {
        creationCheck([&] (const Id<StateModel>& id) { });
    }
    else
    {
        creationCheck([&] (const Id<StateModel>& id) { createToEvent_base(id); });
    }
}

void ScenarioCreation_FromState::createToState()
{
    creationCheck([&] (const Id<StateModel>& id) { createToState_base(id); });
}


