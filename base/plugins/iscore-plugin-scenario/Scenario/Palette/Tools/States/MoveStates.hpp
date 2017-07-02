#pragma once
#include <Scenario/Palette/ScenarioPaletteBaseStates.hpp>
#include <Scenario/Palette/Tools/ScenarioRollbackStrategy.hpp>
#include <Scenario/Palette/Transitions/AnythingTransitions.hpp>
#include <Scenario/Palette/Transitions/ConstraintTransitions.hpp>
#include <Scenario/Palette/Transitions/EventTransitions.hpp>
#include <Scenario/Palette/Transitions/NothingTransitions.hpp>
#include <Scenario/Palette/Transitions/TimeNodeTransitions.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>

#include <QFinalState>
#include <Scenario/Commands/Scenario/Displacement/MoveEventMeta.hpp>
#include <iscore/command/Dispatchers/SingleOngoingCommandDispatcher.hpp>
#include <iscore/locking/ObjectLocker.hpp>
#include <QApplication>
namespace Scenario
{
template<typename T>
class MoveConstraintState final : public StateBase<Scenario::ProcessModel>
{
public:
  MoveConstraintState(
      const T& stateMachine,
      const Scenario::ProcessModel& scenario,
      const iscore::CommandStackFacade& stack,
      iscore::ObjectLocker& locker,
      QState* parent)
      : StateBase<Scenario::ProcessModel>{scenario, parent}, m_movingDispatcher{stack}
  {
    this->setObjectName("MoveConstraintState");
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

      auto t_pressed
          = iscore::make_transition<MoveOnAnything_Transition<Scenario::ProcessModel>>(
              pressed, moving, *this);
      QObject::connect(t_pressed, &QAbstractTransition::triggered, [&]() {

          auto& scenar = stateMachine.model();
          m_initialClick = this->currentPoint;
          if(!this->clickedConstraint)
              return;
          auto& cst = scenario.constraint(*this->clickedConstraint);
          auto& sev = Scenario::startEvent(cst, scenario);

          m_constraintInitialPoint = {cst.startDate(), cst.heightPercentage()};

          auto prev_csts = previousConstraints(sev, scenar);
          if (!prev_csts.empty())
          {
            // We find the one that starts the latest.
            TimeVal t = TimeVal::zero();
            for (const auto& cst_id : prev_csts)
            {
              const auto& other_date = scenar.constraint(cst_id).startDate();
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

      iscore::make_transition<ReleaseOnAnything_Transition>(
          pressed, finalState);
      iscore::make_transition<MoveOnAnything_Transition<Scenario::ProcessModel>>(
          moving, moving, *this);
      iscore::make_transition<ReleaseOnAnything_Transition>(moving, released);

      QObject::connect(moving, &QState::entered, [&] {
          auto& scenario = stateMachine.model();
          if(!this->clickedConstraint)
              return;
          auto& cst = scenario.constraint(*this->clickedConstraint);
          auto& sev = Scenario::startEvent(cst, scenario);

          TimeVal date{};
          if(qApp->keyboardModifiers() & Qt::ShiftModifier)
            date = m_constraintInitialPoint.date + (this->currentPoint.date - m_initialClick.date);
          else
            date = m_constraintInitialPoint.date;
          if(this->m_pressedPrevious)
            date = std::max(date, *this->m_pressedPrevious);
          date = std::max(date, TimeVal{});

          this->m_movingDispatcher.submitCommand(
                      this->m_scenario,
                      sev.id(),
                      date,
                      m_constraintInitialPoint.y + (this->currentPoint.y - m_initialClick.y),
                      stateMachine.editionSettings().expandMode(),
                      cst.startState());
      });

      QObject::connect(
          released, &QState::entered, [&]() {
          m_movingDispatcher.commit();
          m_pressedPrevious = {};
      });
    }

    auto rollbackState = new QState{this};
    iscore::make_transition<iscore::Cancel_Transition>(
        mainState, rollbackState);
    rollbackState->addTransition(finalState);
    QObject::connect(
        rollbackState, &QState::entered, [&]() {
        m_movingDispatcher.rollback();
        m_pressedPrevious = {};
    });

    this->setInitialState(mainState);
  }

  //SingleOngoingCommandDispatcher<MoveConstraintCommand_T> m_dispatcher;
  SingleOngoingCommandDispatcher<Command::MoveEventMeta> m_movingDispatcher;

private:
  Scenario::Point m_initialClick{};
  Scenario::Point m_constraintInitialPoint{};
  optional<TimeVal> m_pressedPrevious;
};

template <
    typename MoveBraceCommand_T, // SetMinDuration or setMaxDuration
    typename Scenario_T, typename ToolPalette_T>
class MoveConstraintBraceState final : public StateBase<Scenario_T>
{
public:
  MoveConstraintBraceState(
      const ToolPalette_T& stateMachine,
      const Scenario_T& scenarioPath,
      const iscore::CommandStackFacade& stack,
      iscore::ObjectLocker& locker,
      QState* parent)
      : StateBase<Scenario_T>{scenarioPath, parent}, m_dispatcher{stack}
  {
    this->setObjectName("MoveConstraintBraceState");
    using namespace Scenario::Command;
    auto finalState = new QFinalState{this};

    auto mainState = new QState{this};
    {
      auto pressed = new QState{mainState};
      auto released = new QState{mainState};
      auto moving = new QState{mainState};

      mainState->setInitialState(pressed);
      released->addTransition(finalState);

      iscore::make_transition<MoveOnAnything_Transition<Scenario_T>>(
          pressed, moving, *this);
      iscore::make_transition<ReleaseOnAnything_Transition>(
          pressed, finalState);

      iscore::make_transition<MoveOnAnything_Transition<Scenario_T>>(
          moving, moving, *this);
      iscore::make_transition<ReleaseOnAnything_Transition>(moving, released);

      QObject::connect(pressed, &QState::entered, [&]() {
        this->m_initialDate = this->currentPoint.date;
        if (this->clickedConstraint)
        {
          auto& scenar = stateMachine.model();
          auto& cstr = scenar.constraint(*this->clickedConstraint);
          this->m_initialDuration
              = ((cstr.duration)
                 .*MoveBraceCommand_T::corresponding_member)(); // = constraint
                                                                // MinDuration
                                                                // or
                                                                // maxDuration
        }
      });

      QObject::connect(moving, &QState::entered, [&]() {
        if (this->clickedConstraint)
        {
          auto& scenar = stateMachine.model();
          auto& cstr = scenar.constraint(*this->clickedConstraint);
          auto date
              = this->currentPoint.date - *m_initialDate + *m_initialDuration;
          this->m_dispatcher.submitCommand(cstr, date, false);
        }
      });

      QObject::connect(
          released, &QState::entered, [&]() { this->m_dispatcher.commit(); });
    }

    auto rollbackState = new QState{this};
    iscore::make_transition<iscore::Cancel_Transition>(
        mainState, rollbackState);
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
    typename MoveTimeNodeCommand_T, // MoveEventMeta
    typename Scenario_T, typename ToolPalette_T>
class MoveTimeNodeState final : public StateBase<Scenario_T>
{
public:
  MoveTimeNodeState(
      const ToolPalette_T& stateMachine,
      const Scenario_T& scenarioPath,
      const iscore::CommandStackFacade& stack,
      iscore::ObjectLocker& locker,
      QState* parent)
      : StateBase<Scenario_T>{scenarioPath, parent}, m_dispatcher{stack}
  {
    this->setObjectName("MoveTimeNodeState");
    using namespace Scenario::Command;
    auto finalState = new QFinalState{this};

    auto mainState = new QState{this};
    {
      auto pressed = new QState{mainState};
      auto released = new QState{mainState};
      auto moving = new QState{mainState};
      mainState->setInitialState(pressed);

      // General setup
      released->addTransition(finalState);

      iscore::make_transition<MoveOnAnything_Transition<Scenario_T>>(
          pressed, moving, *this);
      iscore::make_transition<ReleaseOnAnything_Transition>(
          pressed, finalState);
      iscore::make_transition<MoveOnAnything_Transition<Scenario_T>>(
          moving, moving, *this);
      iscore::make_transition<ReleaseOnAnything_Transition>(moving, released);

      // What happens in each state.
      QObject::connect(pressed, &QState::entered, [&]() {
        if (!this->clickedTimeNode)
          return;

        auto& scenar = stateMachine.model();

        auto prev_csts = previousConstraints(
            scenar.timeNode(*this->clickedTimeNode), scenar);
        if (!prev_csts.empty())
        {
          // We find the one that starts the latest.
          TimeVal t = TimeVal::zero();
          for (const auto& cst_id : prev_csts)
          {
            const auto& other_date = scenar.constraint(cst_id).startDate();
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
        if (!this->clickedTimeNode)
          return;

        // Get the 1st event on the timenode.
        auto& scenar = stateMachine.model();
        auto& tn = scenar.timeNode(*this->clickedTimeNode);
        const auto& ev_id = tn.events().first();
        auto date = this->currentPoint.date;

        if (!stateMachine.editionSettings().sequence())
        {
          // TODO why??
          date = tn.date();
        }

        if (this->m_pressedPrevious)
        {
          date = max(date, *this->m_pressedPrevious);
        }

        m_dispatcher.submitCommand(
            this->m_scenario,
            ev_id,
            date,
            this->currentPoint.y,
            stateMachine.editionSettings().expandMode());
      });

      QObject::connect(
          released, &QState::entered, [&]() { m_dispatcher.commit(); });
    }

    auto rollbackState = new QState{this};
    iscore::make_transition<iscore::Cancel_Transition>(
        mainState, rollbackState);
    rollbackState->addTransition(finalState);
    QObject::connect(
        rollbackState, &QState::entered, [&]() { m_dispatcher.rollback(); });

    this->setInitialState(mainState);
  }

  SingleOngoingCommandDispatcher<MoveTimeNodeCommand_T> m_dispatcher;
  optional<TimeVal> m_pressedPrevious;
};
}
