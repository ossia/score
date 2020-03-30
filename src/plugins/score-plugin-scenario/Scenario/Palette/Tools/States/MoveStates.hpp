#pragma once
#include <Scenario/Commands/Scenario/Displacement/MoveEventMeta.hpp>
#include <Scenario/Palette/ScenarioPaletteBaseStates.hpp>
#include <Scenario/Palette/Tools/ScenarioRollbackStrategy.hpp>
#include <Scenario/Palette/Transitions/AnythingTransitions.hpp>
#include <Scenario/Palette/Transitions/EventTransitions.hpp>
#include <Scenario/Palette/Transitions/IntervalTransitions.hpp>
#include <Scenario/Palette/Transitions/NothingTransitions.hpp>
#include <Scenario/Palette/Transitions/TimeSyncTransitions.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>

#include <score/command/Dispatchers/SingleOngoingCommandDispatcher.hpp>
#include <score/locking/ObjectLocker.hpp>

#include <QApplication>
#include <QFinalState>
namespace Scenario
{
template <typename T>
class MoveIntervalState final : public StateBase<Scenario::ProcessModel>
{
public:
  MoveIntervalState(
      const T& stateMachine,
      const Scenario::ProcessModel& scenario,
      const score::CommandStackFacade& stack,
      score::ObjectLocker& locker,
      QState* parent)
      : StateBase<Scenario::ProcessModel>{scenario, parent}
      , m_movingDispatcher{stack}
  {
    this->setObjectName("MoveIntervalState");
    using namespace Scenario::Command;
    auto finalState = new QFinalState{this};

    auto mainState = new QState{this};
    {
      auto pressed = new QState{mainState};
      auto released = new QState{mainState};
      auto moving = new QState{mainState};

      // General setup
      mainState->setInitialState(pressed);
      released->addTransition(finalState);

      auto t_pressed = score::make_transition<
          MoveOnAnything_Transition<Scenario::ProcessModel>>(
          pressed, moving, *this);
      QObject::connect(t_pressed, &QAbstractTransition::triggered, [&]() {
        auto& scenar = stateMachine.model();
        m_initialClick = this->currentPoint;
        if (!this->clickedInterval)
          return;
        auto& cst = scenario.interval(*this->clickedInterval);
        auto& sev = Scenario::startEvent(cst, scenario);

        m_intervalInitialPoint = {cst.date(), cst.heightPercentage()};

        auto prev_csts = previousNonGraphIntervals(sev, scenar);
        if (!prev_csts.empty())
        {
          // We find the one that starts the latest.
          TimeVal t = TimeVal::zero();
          for (const auto& cst_id : prev_csts)
          {
            const auto& other_date = scenar.interval(cst_id).date();
            if (other_date > t)
              t = other_date;
          }

          // These 10 milliseconds are here to prevent "squashing"
          // processes to zero, which leads to problem (they can't scale back!)
          this->m_pressedPrevious = t + TimeVal::fromMsecs(10);
        }
        else
        {
          this->m_pressedPrevious = ossia::none;
        }
      });

      score::make_transition<ReleaseOnAnything_Transition>(
          pressed, finalState);
      score::make_transition<
          MoveOnAnything_Transition<Scenario::ProcessModel>>(
          moving, moving, *this);
      score::make_transition<ReleaseOnAnything_Transition>(moving, released);

      QObject::connect(moving, &QState::entered, [&] {
        auto& scenario = stateMachine.model();
        if (!this->clickedInterval)
          return;
        auto& cst = scenario.interval(*this->clickedInterval);
        auto& sev = Scenario::startEvent(cst, scenario);

        TimeVal date{};
        if (qApp->keyboardModifiers() & Qt::ShiftModifier)
          date = m_intervalInitialPoint.date
                 + (this->currentPoint.date - m_initialClick.date);
        else
          date = m_intervalInitialPoint.date;
        if (this->m_pressedPrevious)
          date = std::max(date, *this->m_pressedPrevious);

        date = stateMachine.magnetic().getPosition(&stateMachine.model(), date);
        date = std::max(date, TimeVal{});

        this->m_movingDispatcher.submit(
            this->m_scenario,
            sev.id(),
            date,
            m_intervalInitialPoint.y
                + (this->currentPoint.y - m_initialClick.y),
            stateMachine.editionSettings().expandMode(),
            stateMachine.editionSettings().lockMode(),
            cst.startState());
      });

      QObject::connect(released, &QState::entered, [&]() {
        m_movingDispatcher.commit();
        m_pressedPrevious = {};
      });
    }

    auto rollbackState = new QState{this};
    score::make_transition<score::Cancel_Transition>(mainState, rollbackState);
    rollbackState->addTransition(finalState);
    QObject::connect(rollbackState, &QState::entered, [&]() {
      m_movingDispatcher.rollback();
      m_pressedPrevious = {};
    });

    this->setInitialState(mainState);
  }

  // SingleOngoingCommandDispatcher<MoveIntervalCommand_T> m_dispatcher;
  SingleOngoingCommandDispatcher<Command::MoveEventMeta> m_movingDispatcher;

private:
  Scenario::Point m_initialClick{};
  Scenario::Point m_intervalInitialPoint{};
  optional<TimeVal> m_pressedPrevious;
};

template <
    typename MoveBraceCommand_T, // SetMinDuration or setMaxDuration
    typename Scenario_T,
    typename ToolPalette_T>
class MoveIntervalBraceState final : public StateBase<Scenario_T>
{
public:
  MoveIntervalBraceState(
      const ToolPalette_T& stateMachine,
      const Scenario_T& scenarioPath,
      const score::CommandStackFacade& stack,
      score::ObjectLocker& locker,
      QState* parent)
      : StateBase<Scenario_T>{scenarioPath, parent}, m_dispatcher{stack}
  {
    this->setObjectName("MoveIntervalBraceState");
    using namespace Scenario::Command;
    auto finalState = new QFinalState{this};

    auto mainState = new QState{this};
    {
      auto pressed = new QState{mainState};
      auto released = new QState{mainState};
      auto moving = new QState{mainState};

      mainState->setInitialState(pressed);
      released->addTransition(finalState);

      score::make_transition<MoveOnAnything_Transition<Scenario_T>>(
          pressed, moving, *this);
      score::make_transition<ReleaseOnAnything_Transition>(
          pressed, finalState);

      score::make_transition<MoveOnAnything_Transition<Scenario_T>>(
          moving, moving, *this);
      score::make_transition<ReleaseOnAnything_Transition>(moving, released);

      QObject::connect(pressed, &QState::entered, [&]() {
        this->m_initialDate = this->currentPoint.date;
        if (this->clickedInterval)
        {
          auto& scenar = stateMachine.model();
          auto& cstr = scenar.interval(*this->clickedInterval);
          this->m_initialDuration
              = ((cstr.duration)
                 .*MoveBraceCommand_T::corresponding_member)(); // = interval
                                                                // MinDuration
                                                                // or
                                                                // maxDuration
        }
      });

      QObject::connect(moving, &QState::entered, [&]() {
        if (this->clickedInterval)
        {
          auto& scenar = stateMachine.model();
          auto& cstr = scenar.interval(*this->clickedInterval);
          auto date
              = this->currentPoint.date - *m_initialDate + *m_initialDuration;

          date = stateMachine.magnetic().getPosition(&stateMachine.model(), date);

          this->m_dispatcher.submit(cstr, date, false);
        }
      });

      QObject::connect(
          released, &QState::entered, [&]() { this->m_dispatcher.commit(); });
    }

    auto rollbackState = new QState{this};
    score::make_transition<score::Cancel_Transition>(mainState, rollbackState);
    rollbackState->addTransition(finalState);
    QObject::connect(
        rollbackState, &QState::entered, [&]() { m_dispatcher.rollback(); });

    this->setInitialState(mainState);
  }
  SingleOngoingCommandDispatcher<MoveBraceCommand_T> m_dispatcher;

private:
  optional<TimeVal> m_initialDate;
  optional<TimeVal> m_initialDuration;
};

template <
    typename MoveTimeSyncCommand_T, // MoveEventMeta
    typename Scenario_T,
    typename ToolPalette_T>
class MoveTimeSyncState final : public StateBase<Scenario_T>
{
public:
  MoveTimeSyncState(
      const ToolPalette_T& stateMachine,
      const Scenario_T& scenarioPath,
      const score::CommandStackFacade& stack,
      score::ObjectLocker& locker,
      QState* parent)
      : StateBase<Scenario_T>{scenarioPath, parent}, m_dispatcher{stack}
  {
    this->setObjectName("MoveTimeSyncState");
    using namespace Scenario::Command;
    auto finalState = new QFinalState{this};

    auto mainState = new QState{this};
    {
      auto pressed = new QState{mainState};
      auto released = new QState{mainState};
      auto moving = new QState{mainState};

      // General setup
      mainState->setInitialState(pressed);
      released->addTransition(finalState);

      score::make_transition<MoveOnAnything_Transition<Scenario_T>>(
          pressed, moving, *this);
      score::make_transition<ReleaseOnAnything_Transition>(
          pressed, finalState);
      score::make_transition<MoveOnAnything_Transition<Scenario_T>>(
          moving, moving, *this);
      score::make_transition<ReleaseOnAnything_Transition>(moving, released);

      // What happens in each state.
      QObject::connect(pressed, &QState::entered, [&]() {
        if (!this->clickedTimeSync)
          return;

        auto& scenar = stateMachine.model();

        auto prev_csts = previousNonGraphIntervals(
            scenar.timeSync(*this->clickedTimeSync), scenar);
        if (!prev_csts.empty())
        {
          // We find the one that starts the latest.
          TimeVal t = TimeVal::zero();
          for (const auto& cst_id : prev_csts)
          {
            const auto& other_date = scenar.interval(cst_id).date();
            if (other_date > t)
              t = other_date;
          }
          this->m_pressedPrevious = t;
        }
        else
        {
          this->m_pressedPrevious = ossia::none;
        }
      });

      QObject::connect(moving, &QState::entered, [&]() {
        if (!this->clickedTimeSync)
          return;

        // Get the 1st event on the timesync.
        auto& scenar = stateMachine.model();
        auto& tn = scenar.timeSync(*this->clickedTimeSync);
        SCORE_ASSERT(!tn.events().empty());
        const auto& ev_id = tn.events().front();
        auto date = this->currentPoint.date;

        date = stateMachine.magnetic().getPosition(&stateMachine.model(), date);

        if (!stateMachine.editionSettings().sequence())
        {
          // TODO why??
          date = tn.date();
        }

        if (this->m_pressedPrevious)
        {
          date = max(date, *this->m_pressedPrevious);
        }

        m_dispatcher.submit(
            this->m_scenario,
            ev_id,
            date,
            this->currentPoint.y,
            stateMachine.editionSettings().expandMode(),
            stateMachine.editionSettings().lockMode());
      });

      QObject::connect(
          released, &QState::entered, [&]() {
        m_dispatcher.commit();
        m_pressedPrevious = {};
      });
    }

    auto rollbackState = new QState{this};
    score::make_transition<score::Cancel_Transition>(mainState, rollbackState);
    rollbackState->addTransition(finalState);
    QObject::connect(
        rollbackState, &QState::entered, [&]() { m_dispatcher.rollback(); });

    this->setInitialState(mainState);
  }

  SingleOngoingCommandDispatcher<MoveTimeSyncCommand_T> m_dispatcher;
  optional<TimeVal> m_pressedPrevious;
};
}
