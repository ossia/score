#pragma once

#include <Scenario/Commands/Scenario/Creations/CreateEvent_State.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateState.hpp>
#include <Scenario/Commands/Scenario/Displacement/MoveNewEvent.hpp>
#include <Scenario/Commands/Scenario/Displacement/MoveNewState.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Palette/Tools/States/ScenarioCreationState.hpp>
#include <Scenario/Palette/Transitions/AnythingTransitions.hpp>
#include <Scenario/Palette/Transitions/EventTransitions.hpp>
#include <Scenario/Palette/Transitions/IntervalTransitions.hpp>
#include <Scenario/Palette/Transitions/NothingTransitions.hpp>
#include <Scenario/Palette/Transitions/StateTransitions.hpp>
#include <Scenario/Palette/Transitions/TimeSyncTransitions.hpp>

#include <QFinalState>

namespace Scenario
{
template <typename Scenario_T, typename ToolPalette_T>
class Creation_FromEvent final : public CreationState<Scenario_T, ToolPalette_T>
{
public:
  Creation_FromEvent(
      const ToolPalette_T& stateMachine,
      const Scenario_T& scenarioPath,
      const score::CommandStackFacade& stack,
      QState* parent)
      : CreationState<Scenario_T, ToolPalette_T>{
          stateMachine,
          stack,
          std::move(scenarioPath),
          parent}
  {
    using namespace Scenario::Command;
    auto finalState = new QFinalState{this};
    QObject::connect(finalState, &QState::entered, [&]() { this->clearCreatedIds(); });

    auto mainState = new QState{this};
    mainState->setObjectName("Main state");
    {
      auto pressed = new QState{mainState};
      auto released = new QState{mainState};
      auto move_nothing = new StrongQState<MoveOnNothing>{mainState};
      auto move_state = new StrongQState<MoveOnState>{mainState};
      auto move_event = new StrongQState<MoveOnEvent>{mainState};
      auto move_timesync = new StrongQState<MoveOnTimeSync>{mainState};

      pressed->setObjectName("Pressed");
      released->setObjectName("Released");
      move_nothing->setObjectName("Move on Nothing");
      move_state->setObjectName("Move on State");
      move_event->setObjectName("Move on Event");
      move_timesync->setObjectName("Move on Sync");

      // General setup
      mainState->setInitialState(pressed);
      released->addTransition(finalState);

      // Release
      score::make_transition<ReleaseOnAnything_Transition>(mainState, released);

      // Pressed -> ...
      score::make_transition<MoveOnNothing_Transition<Scenario_T>>(pressed, move_nothing, *this);

      /// MoveOnNothing -> ...
      // MoveOnNothing -> MoveOnNothing.
      score::make_transition<MoveOnNothing_Transition<Scenario_T>>(
          move_nothing, move_nothing, *this);

      // MoveOnNothing -> MoveOnState.
      this->add_transition(move_nothing, move_state, [&]() {
        this->rollback();
        createToState();
      });

      // MoveOnNothing -> MoveOnEvent.
      this->add_transition(move_nothing, move_event, [&]() {
        this->rollback();
        createToEvent();
      });

      // MoveOnNothing -> MoveOnTimeSync
      this->add_transition(move_nothing, move_timesync, [&]() {
        this->rollback();
        createToTimeSync();
      });

      /// MoveOnState -> ...
      // MoveOnState -> MoveOnNothing
      this->add_transition(move_state, move_nothing, [&]() {
        this->rollback();
        createToNothing();
      });

      // MoveOnState -> MoveOnState
      // We don't do anything, the interval should not move.

      // MoveOnState -> MoveOnEvent
      this->add_transition(move_state, move_event, [&]() {
        this->rollback();
        createToEvent();
      });

      // MoveOnState -> MoveOnTimeSync
      this->add_transition(move_state, move_timesync, [&]() {
        this->rollback();
        createToTimeSync();
      });

      /// MoveOnEvent -> ...
      // MoveOnEvent -> MoveOnNothing
      this->add_transition(move_event, move_nothing, [&]() {
        this->rollback();
        createToNothing();
      });

      // MoveOnEvent -> MoveOnState
      this->add_transition(move_event, move_state, [&]() {
        this->rollback();
        createToState();
      });

      // MoveOnEvent -> MoveOnEvent
      score::make_transition<MoveOnEvent_Transition<Scenario_T>>(move_event, move_event, *this);

      // MoveOnEvent -> MoveOnTimeSync
      this->add_transition(move_event, move_timesync, [&]() {
        this->rollback();
        createToTimeSync();
      });

      /// MoveOnTimeSync -> ...
      // MoveOnTimeSync -> MoveOnNothing
      this->add_transition(move_timesync, move_nothing, [&]() {
        this->rollback();
        createToNothing();
      });

      // MoveOnTimeSync -> MoveOnState
      this->add_transition(move_timesync, move_state, [&]() {
        this->rollback();
        createToState();
      });

      // MoveOnTimeSync -> MoveOnEvent
      this->add_transition(move_timesync, move_event, [&]() {
        this->rollback();
        createToEvent();
      });

      // MoveOnTimeSync -> MoveOnTimeSync
      score::make_transition<MoveOnTimeSync_Transition<Scenario_T>>(
          move_timesync, move_timesync, *this);

      // What happens in each state.
      QObject::connect(pressed, &QState::entered, [&]() {
        this->m_clickedPoint = this->currentPoint;
        // Create a simple state where we are

        createInitialState();
        // createToNothing();
      });

      QObject::connect(move_nothing, &QState::entered, [&]() {
        if (this->createdIntervals.empty() || this->createdEvents.empty())
        {
          this->rollback();
          return;
        }

        if (this->currentPoint.date <= this->m_clickedPoint.date)
        {
          this->currentPoint.date = this->m_clickedPoint.date + TimeVal::fromMsecs(10);
          ;
        }

        this->currentPoint.date
            = stateMachine.magnetic().getPosition(&stateMachine.model(), this->currentPoint.date);

        this->m_dispatcher.template submit<MoveNewEvent>(
            this->m_scenario,
            this->createdIntervals.last(),
            this->createdEvents.last(),
            this->currentPoint.date,
            this->currentPoint.y,
            stateMachine.editionSettings().tool() == Tool::CreateSequence);
      });

      QObject::connect(move_timesync, &QState::entered, [&]() {
        if (this->createdStates.empty())
        {
          this->rollback();
          return;
        }

        if (this->currentPoint.date <= this->m_clickedPoint.date)
        {
          return;
        }

        this->m_dispatcher.template submit<MoveNewState>(
            this->m_scenario, this->createdStates.last(), this->currentPoint.y);
      });

      QObject::connect(move_event, &QState::entered, [&]() {
        if (this->createdStates.empty())
        {
          this->rollback();
          return;
        }

        if (this->currentPoint.date <= this->m_clickedPoint.date)
        {
          return;
        }

        this->m_dispatcher.template submit<MoveNewState>(
            this->m_scenario, this->createdStates.last(), this->currentPoint.y);
      });

      QObject::connect(released, &QState::entered, this, &Creation_FromEvent::commit);
    }

    auto rollbackState = new QState{this};
    rollbackState->setObjectName("Rollback");
    score::make_transition<score::Cancel_Transition>(mainState, rollbackState);
    rollbackState->addTransition(finalState);
    QObject::connect(rollbackState, &QState::entered, this, &Creation_FromEvent::rollback);

    this->setInitialState(mainState);
  }

private:
  void createInitialState()
  {
    if (this->clickedEvent)
    {
      auto cmd = new Scenario::Command::CreateState{
          this->m_scenario, *this->clickedEvent, this->currentPoint.y};
      this->m_dispatcher.submit(cmd);

      this->createdStates.append(cmd->createdState());
    }
  }

  void createToNothing()
  {
    createInitialState();
    this->createToNothing_base(this->createdStates.first());
  }

  void createToState()
  {
    createInitialState();
    this->createToState_base(this->createdStates.first());
  }

  // Note : clickedEvent is set at startEvent if clicking in the background.
  void createToEvent()
  {
    if (this->hoveredEvent != this->clickedEvent)
    {
      createInitialState();
      this->createToEvent_base(this->createdStates.first());
    }
  }

  void createToTimeSync()
  {
    createInitialState();
    this->createToTimeSync_base(this->createdStates.first());
  }
};
}
