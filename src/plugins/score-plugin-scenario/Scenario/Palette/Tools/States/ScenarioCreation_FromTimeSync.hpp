#pragma once
#include "ScenarioCreationState.hpp"

#include <Scenario/Commands/Scenario/Creations/CreateInterval.hpp>
#include <Scenario/Commands/Scenario/Displacement/MoveEventMeta.hpp>
#include <Scenario/Commands/Scenario/Displacement/MoveNewEvent.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Palette/Tools/ScenarioRollbackStrategy.hpp>
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
class Creation_FromTimeSync final : public CreationState<Scenario_T, ToolPalette_T>
{
public:
  Creation_FromTimeSync(
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
    {
      auto pressed = new QState{mainState};
      auto released = new QState{mainState};
      auto move_nothing = new StrongQState<MoveOnNothing>{mainState};
      auto move_state = new StrongQState<MoveOnState>{mainState};
      auto move_event = new StrongQState<MoveOnEvent>{mainState};
      auto move_timesync = new StrongQState<MoveOnTimeSync>{mainState};

      // General setup
      mainState->setInitialState(pressed);
      released->addTransition(finalState);

      // Release
      score::make_transition<ReleaseOnAnything_Transition>(mainState, released);

      // Pressed -> ...
      auto t_pressed_moving_nothing = score::make_transition<MoveOnNothing_Transition<Scenario_T>>(
          pressed, move_nothing, *this);

      QObject::connect(t_pressed_moving_nothing, &QAbstractTransition::triggered, [&]() {
        this->rollback();
        createToNothing();
      });

      /// MoveOnNothing -> ...
      // MoveOnNothing -> MoveOnNothing.
      score::make_transition<MoveOnNothing_Transition<Scenario_T>>(
          move_nothing, move_nothing, *this);

      // MoveOnNothing -> MoveOnState.
      this->add_transition(move_nothing, move_state, [&]() {
        this->rollback();
        SCORE_TODO;
      });

      // MoveOnNothing -> MoveOnEvent.
      this->add_transition(move_nothing, move_event, [&]() {
        if (this->hoveredEvent && this->createdEvents.contains(*this->hoveredEvent))
        {
          return;
        }
        this->rollback();

        createToEvent();
      });

      // MoveOnNothing -> MoveOnTimeSync
      this->add_transition(move_nothing, move_timesync, [&]() {
        if (this->hoveredTimeSync && this->createdTimeSyncs.contains(*this->hoveredTimeSync))
        {
          return;
        }
        this->rollback();
        createToTimeSync();
      });

      /// MoveOnState -> ...
      // MoveOnState -> MoveOnNothing
      this->add_transition(move_state, move_nothing, [&]() {
        this->rollback();
        SCORE_TODO;
      });

      // MoveOnState -> MoveOnState
      // We don't do anything, the interval should not move.

      // MoveOnState -> MoveOnEvent
      this->add_transition(move_state, move_event, [&]() {
        this->rollback();
        SCORE_TODO;
      });

      // MoveOnState -> MoveOnTimeSync
      this->add_transition(move_state, move_timesync, [&]() {
        this->rollback();
        SCORE_TODO;
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
        SCORE_TODO;
      });

      // MoveOnEvent -> MoveOnEvent
      score::make_transition<MoveOnEvent_Transition<Scenario_T>>(move_event, move_event, *this);

      // MoveOnEvent -> MoveOnTimeSync
      this->add_transition(move_event, move_timesync, [&]() {
        if (this->hoveredTimeSync && this->createdTimeSyncs.contains(*this->hoveredTimeSync))
        {
          return;
        }
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
        SCORE_TODO;
      });

      // MoveOnTimeSync -> MoveOnEvent
      this->add_transition(move_timesync, move_event, [&]() {
        if (this->hoveredEvent && this->createdEvents.contains(*this->hoveredEvent))
        {
          this->rollback();
          return;
        }
        this->rollback();
        createToEvent();
      });

      // MoveOnTimeSync -> MoveOnTimeSync
      score::make_transition<MoveOnTimeSync_Transition<Scenario_T>>(
          move_timesync, move_timesync, *this);

      // What happens in each state.
      QObject::connect(pressed, &QState::entered, [&]() {
        this->m_clickedPoint = this->currentPoint;
        createInitialEventAndState();
      });

      QObject::connect(move_nothing, &QState::entered, [&]() {
        if (this->createdEvents.empty() || this->createdIntervals.empty())
        {
          this->rollback();
          return;
        }

        this->currentPoint.date
            = stateMachine.magnetic().getPosition(&stateMachine.model(), this->currentPoint.date);

        if (this->currentPoint.date <= this->m_clickedPoint.date)
        {
          this->currentPoint.date = this->m_clickedPoint.date + TimeVal::fromMsecs(10);
          ;
        }

        // Move the timesync
        this->m_dispatcher.template submit<MoveNewEvent>(
            this->m_scenario,
            this->createdIntervals.last(),
            this->createdEvents.last(),
            this->currentPoint.date,
            this->currentPoint.y,
            stateMachine.editionSettings().sequence());
      });

      QObject::connect(move_timesync, &QState::entered, [&]() {
        if (this->createdEvents.empty())
        {
          this->rollback();
          return;
        }

        if (this->currentPoint.date <= this->m_clickedPoint.date)
        {
          return;
        }

        this->m_dispatcher.template submit<MoveEventMeta>(
            this->m_scenario,
            this->createdEvents.last(),
            TimeVal::zero(),
            0.,
            stateMachine.editionSettings().expandMode(),
            LockMode::Free);
      });

      QObject::connect(released, &QState::entered, this, &Creation_FromTimeSync::commit);
    }

    auto rollbackState = new QState{this};
    score::make_transition<score::Cancel_Transition>(mainState, rollbackState);
    rollbackState->addTransition(finalState);
    QObject::connect(rollbackState, &QState::entered, this, &Creation_FromTimeSync::rollback);

    this->setInitialState(mainState);
  }

private:
  void createInitialEventAndState()
  {
    if (this->clickedTimeSync)
    {
      auto cmd = new Command::CreateEvent_State{
          this->m_scenario, *this->clickedTimeSync, this->currentPoint.y};
      this->m_dispatcher.submit(cmd);

      this->createdStates.append(cmd->createdState());
      this->createdEvents.append(cmd->createdEvent());
    }
  }

  void createToNothing()
  {
    createInitialEventAndState();
    this->createToNothing_base(this->createdStates.first());
  }
  void createToState()
  {
    createInitialEventAndState();
    this->createToState_base(this->createdStates.first());
  }
  void createToEvent()
  {
    createInitialEventAndState();
    this->createToEvent_base(this->createdStates.first());
  }
  void createToTimeSync()
  {
    // TODO "if hoveredTimeSync != clickedTimeSync"
    createInitialEventAndState();
    this->createToTimeSync_base(this->createdStates.first());
  }
};
}
