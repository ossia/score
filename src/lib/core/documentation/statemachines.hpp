#pragma once
/*! \page StateMachines State Machines
 * For complex real-time editing possibilities, like what happens in the scenario and the curve,
 * we use the QStateMachine framework.
 *
 * However due to the complexity of the state machines involved, a typesafe layer is added upon it, which allows
 * to easily have matched events and transitions at no more runtime cost than what Qt offers.
 *
 * There are default events transitions for the common cases of objects being pressed, moving, released, with an alternate modifier, and a cancel option.
 *
 * The general pattern relies on multiple encapsulated state machines :
 *
 * * A top-level one that will handle the current "tool" (move, create, etc...)
 * * Each tool is a state which encapsulates another state machine.
 *   This is because we want to be able to change the active tool while currently doing something in a sub-state,
 *   which is not possible if all the states are part of the state machine.
 *
 * Instead of relying on QGraphicsItem's facilities for knowing if an object is pressed / moved / etc.,
 * we use the "itemUnderMouse" method and a switch/case which is faster by a large amount.
 *
 * Each tool state will then have cases for what happens when an object is entered and left for another object, like `ScenarioCreation_FromState` for instance.
 *
 * For simpler cases, see `MoveState` in the automation plug-in, which currently handles moving a point.
 */
