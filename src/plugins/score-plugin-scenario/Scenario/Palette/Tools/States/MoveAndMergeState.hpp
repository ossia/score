#pragma once

#include <Scenario/Commands/Scenario/Displacement/MoveEventMeta.hpp>
#include <Scenario/Commands/Scenario/Merge/MergeEvents.hpp>
#include <Scenario/Commands/Scenario/Merge/MergeTimeSyncs.hpp>
#include <Scenario/Document/Event/EventPresenter.hpp>
#include <Scenario/Document/Event/EventView.hpp>
#include <Scenario/Document/State/StatePresenter.hpp>
#include <Scenario/Document/State/StateView.hpp>
#include <Scenario/Document/TimeSync/TimeSyncPresenter.hpp>
#include <Scenario/Document/TimeSync/TimeSyncView.hpp>
#include <Scenario/Document/TimeSync/TriggerView.hpp>
#include <Scenario/Palette/ScenarioPaletteBaseStates.hpp>
#include <Scenario/Palette/Tools/ScenarioRollbackStrategy.hpp>
#include <Scenario/Palette/Transitions/AnythingTransitions.hpp>
#include <Scenario/Palette/Transitions/EventTransitions.hpp>
#include <Scenario/Palette/Transitions/NothingTransitions.hpp>
#include <Scenario/Palette/Transitions/StateTransitions.hpp>
#include <Scenario/Palette/Transitions/TimeSyncTransitions.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>

#include <score/command/Dispatchers/MultiOngoingCommandDispatcher.hpp>
#include <score/locking/ObjectLocker.hpp>

#include <Magnetism/MagnetismAdjuster.hpp>
//#include <Scenario/Application/ScenarioValidity.hpp>
#include <QFinalState>

namespace Scenario
{
class ToolPalette;
/*
template <typename TheCommand>
class BugfixDispatcher final : public ICommandDispatcher
{
public:
  const Scenario::ProcessModel& scenario;
  BugfixDispatcher(const score::CommandStackFacade& stack
                                 , const Scenario::ProcessModel& p)
      : ICommandDispatcher{stack}
      , scenario{p}
  {
  }

  template <typename... Args>
  void submit(Args&&... args)
  {
    if (!m_cmd)
    {
      stack().disableActions();
      m_cmd = std::make_unique<TheCommand>(std::forward<Args>(args)...);
      ScenarioValidityChecker::checkValidity(scenario);
      m_cmd->redo(stack().context());
      ScenarioValidityChecker::checkValidity(scenario);
    }
    else
    {
      m_cmd->update(std::forward<Args>(args)...);
      ScenarioValidityChecker::checkValidity(scenario);
      m_cmd->redo(stack().context());
      ScenarioValidityChecker::checkValidity(scenario);
    }
  }

  void commit()
  {
    if (m_cmd)
    {
      SendStrategy::Quiet::send(stack(), m_cmd.release());
      stack().enableActions();
      ScenarioValidityChecker::checkValidity(scenario);
    }
  }

  void rollback()
  {
      ScenarioValidityChecker::checkValidity(scenario);
    if (m_cmd)
    {
      m_cmd->undo(stack().context());
      ScenarioValidityChecker::checkValidity(scenario);
      stack().enableActions();
    }
    m_cmd.reset();
  }

private:
  std::unique_ptr<TheCommand> m_cmd;
};
*/
template <
    typename MoveEventCommand_T, // MoveEventMeta
    typename Scenario_T,
    typename ToolPalette_T>
class MoveEventState final : public StateBase<Scenario_T>
{
public:
  MoveEventState(
      const ToolPalette_T& stateMachine,
      const Scenario_T& scenarioPath,
      const score::CommandStackFacade& stack,
      score::ObjectLocker& locker,
      QState* parent)
      : StateBase<Scenario_T>{scenarioPath, parent}, m_sm{stateMachine}, m_movingDispatcher{stack}
  {
    this->setObjectName("MoveEventState");
    using namespace Scenario::Command;
    auto finalState = new QFinalState{this};

    auto mainState = new QState{this};
    {
      auto pressed = new QState{mainState};
      auto released = new QState{mainState};

      auto onlyMoving = new QState{mainState};

      // General setup
      mainState->setInitialState(pressed);
      released->addTransition(finalState);

      // ***************************************
      // transitions

      // press
      score::make_transition<MoveOnAnything_Transition<Scenario_T>>(pressed, onlyMoving, *this);
      score::make_transition<ReleaseOnAnything_Transition>(pressed, finalState);

      // update commands
      score::make_transition<MoveOnAnything_Transition<Scenario_T>>(onlyMoving, onlyMoving, *this);

      // commit merging
      score::make_transition<ReleaseOnAnything_Transition>(onlyMoving, released);

      // ********************************************
      // What happens in each state.

      QObject::connect(pressed, &QState::entered, [&]() {
        auto& scenar = stateMachine.model();
        auto evId{this->clickedEvent};
        if (!bool(evId) && bool(this->clickedState))
        {
          const Scenario::StateModel& st = scenar.state(*this->clickedState);
          evId = st.eventId();
          m_origPos.y = st.heightPercentage();
        }

        if (!evId)
          return;

        const Scenario::EventModel& ev = scenar.event(*evId);
        m_origPos.date = ev.date();

        auto prev_csts = previousNonGraphIntervals(ev, scenar);
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
          this->m_pressedPrevious = std::nullopt;
        }

        this->m_pressPos = this->currentPoint;
      });

      QObject::connect(onlyMoving, &QState::entered, [&]() {
        auto& scenar = stateMachine.model();
        // If we came here through a state.
        auto evId = this->clickedEvent;
        if (!bool(evId) && bool(this->clickedState))
        {
          evId = scenar.state(*this->clickedState).eventId();
        }

        if (!evId)
          return;

        TimeVal adjDate = this->m_origPos.date + (this->currentPoint.date - this->m_pressPos.date);
        m_lastDate
            = this->m_pressedPrevious ? std::max(adjDate, *this->m_pressedPrevious) : adjDate;

        m_lastDate = stateMachine.magnetic().getPosition(
            &Scenario::parentTimeSync(*evId, stateMachine.model()), m_lastDate);

        m_lastDate = std::max(m_lastDate, TimeVal{});

        if (this->clickedState)
        {
          auto new_y = m_origPos.y + (this->currentPoint.y - this->m_pressPos.y);
          this->m_movingDispatcher.template submit<MoveEventCommand_T>(
              this->m_scenario,
              *evId,
              m_lastDate,
              new_y,
              stateMachine.editionSettings().expandMode(),
              stateMachine.editionSettings().lockMode(),
              *this->clickedState);
        }
        else
        {
          this->m_movingDispatcher.template submit<MoveEventCommand_T>(
              this->m_scenario,
              *evId,
              m_lastDate,
              this->currentPoint.y,
              stateMachine.editionSettings().expandMode(),
              stateMachine.editionSettings().lockMode());
        }
      });

      QObject::connect(released, &QState::entered, [&] {
        if constexpr (std::is_same_v<Scenario::ToolPalette, ToolPalette_T>)
        {
          if (this->clickedState)
          {
            auto& st = this->m_scenario.state(*this->clickedState);
            merge(st, this->m_lastDate);
          }
        }

        m_movingDispatcher.template commit<Command::MoveStateMacro>();
        this->m_pressPos = {};
        this->m_pressedPrevious = {};
      });
    }

    auto rollbackState = new QState{this};
    score::make_transition<score::Cancel_Transition>(mainState, rollbackState);
    rollbackState->addTransition(finalState);
    QObject::connect(rollbackState, &QState::entered, [&] {
      this->rollback();
      this->m_pressPos = {};
      this->m_pressedPrevious = {};
    });

    this->setInitialState(mainState);
  }

  void rollback() { m_movingDispatcher.template rollback<DefaultRollbackStrategy>(); }

  void merge(const StateModel& st, TimeVal date)
  {
    auto& ev = Scenario::parentEvent(st, this->m_scenario);
    auto& ts = Scenario::parentTimeSync(ev, this->m_scenario);

    auto& sst_pres = m_sm.presenter().state(st.id());
    auto& sev_pres = m_sm.presenter().event(ev.id());
    auto& sts_pres = m_sm.presenter().timeSync(ts.id());

    std::vector<QGraphicsItem*> toIgnore;
    toIgnore.push_back(sst_pres.view());
    toIgnore.push_back(sev_pres.view());
    toIgnore.push_back(sts_pres.view());
    toIgnore.push_back(&sts_pres.trigger());
    QGraphicsItem* item = m_sm.itemAt({date, st.heightPercentage()}, toIgnore);

    if (auto stateToMerge = qgraphicsitem_cast<Scenario::StateView*>(item))
    {
      // this->rollback();
      this->m_movingDispatcher.template submit<Command::MergeEvents>(
          this->m_scenario,
          ev.id(),
          Scenario::parentEvent(stateToMerge->presenter().model().id(), this->m_scenario).id());
    }
    else if (auto eventToMerge = qgraphicsitem_cast<Scenario::EventView*>(item))
    {
      // this->rollback();
      this->m_movingDispatcher.template submit<Command::MergeEvents>(
          this->m_scenario, ev.id(), eventToMerge->presenter().model().id());
    }
    else if (auto syncToMerge = qgraphicsitem_cast<Scenario::TimeSyncView*>(item))
    {
      // this->rollback();
      this->m_movingDispatcher.template submit<Command::MergeTimeSyncs>(
          this->m_scenario, ts.id(), syncToMerge->presenter().model().id());
    }
  }

  const ToolPalette_T& m_sm;
  MultiOngoingCommandDispatcher m_movingDispatcher;
  Scenario::Point m_pressPos{}; // where the click landed in the scenario
  Scenario::Point m_origPos{};  // original position of the object being moved
  std::optional<TimeVal> m_pressedPrevious;
  TimeVal m_lastDate;
  bool m_startEventCanBeMerged{};
  bool m_endEventCanBeMerged{};
};

///**
// * Specialization for the ScenarioModel allows merging
// */

// template <
//    typename MoveEventCommand_T, // MoveEventMeta
//    typename ToolPalette_T>
// class MoveEventState<MoveEventCommand_T, Scenario::ProcessModel,
// ToolPalette_T>
//    final : public StateBase<Scenario::ProcessModel>
//{
// public:
//  MoveEventState(
//      const ToolPalette_T& stateMachine,
//      const Scenario::ProcessModel& scenarioPath,
//      const score::CommandStackFacade& stack,
//      score::ObjectLocker& locker,
//      QState* parent)
//      : StateBase<Scenario::ProcessModel>{scenarioPath, parent}
//      , m_movingDispatcher{stack}
//      , m_mergingTnDispatcher{stack}
//      , m_mergingEventDispatcher{stack}
//  {
//    this->setObjectName("MoveEventState");
//    using namespace Scenario::Command;
//    auto finalState = new QFinalState{this};

//    auto mainState = new QState{this};
//    {
//      auto pressed = new QState{mainState};
//      auto released = new QState{mainState};

//      auto onlyMoving = new QState{mainState};
//      auto mergingOnTimeSync = new QState{mainState};
//      auto mergingOnEvent = new QState{mainState};

//      auto rollbackTnMerging = new QState{mainState};
//      auto rollbackEventMerging = new QState{mainState};

//      // General setup
//      mainState->setInitialState(pressed);
//      released->addTransition(finalState);

//      rollbackTnMerging->addTransition(onlyMoving);
//      rollbackEventMerging->addTransition(onlyMoving);

//      // ***************************************
//      // transitions

//      // press
//      score::
//          make_transition<MoveOnAnything_Transition<Scenario::ProcessModel>>(
//              pressed, onlyMoving, *this);
//      score::make_transition<ReleaseOnAnything_Transition>(
//          pressed, finalState);

//      // update commands
//      // score::make_transition<MoveOnAnything_Transition<Scenario_T>>(
//      //                    onlyMoving, onlyMoving, *this);
//      //*
//      score::
//          make_transition<MoveOnAnythingButPonctual_Transition<Scenario::
//                                                                   ProcessModel>>(
//              onlyMoving, onlyMoving, *this);

//      score::
//          make_transition<MoveOnTimeSync_Transition<Scenario::ProcessModel>>(
//              onlyMoving, mergingOnTimeSync, *this);

//      score::make_transition<MoveOnEvent_Transition<Scenario::ProcessModel>>(
//          onlyMoving, mergingOnEvent, *this);
//      score::make_transition<MoveOnEvent_Transition<Scenario::ProcessModel>>(
//                  mergingOnEvent, mergingOnEvent, *this);

//      score::make_transition<MoveOnState_Transition<Scenario::ProcessModel>>(
//          onlyMoving, mergingOnEvent, *this);
//      score::make_transition<MoveOnState_Transition<Scenario::ProcessModel>>(
//                  mergingOnEvent, mergingOnEvent, *this);
//      //*/
//      // rollback merging
//      score::
//          make_transition<MoveOnAnythingButTimeSync_Transition<Scenario::
//                                                                   ProcessModel>>(
//              mergingOnTimeSync, rollbackTnMerging, *this);
//      score::
//          make_transition<MoveOnAnythingButEvent_Transition<Scenario::
//                                                                ProcessModel>>(
//              mergingOnEvent, rollbackEventMerging, *this);

//      // commit merging
//      score::make_transition<ReleaseOnAnything_Transition>(
//          mergingOnTimeSync, released);
//      score::make_transition<ReleaseOnAnything_Transition>(
//          mergingOnEvent, released);
//      score::make_transition<ReleaseOnAnything_Transition>(
//          onlyMoving, released);

//      // ********************************************
//      // What happens in each state.

//      QObject::connect(mergingOnTimeSync, &QState::entered, [&]() {
//          qDebug("mergingOnTimenode");

//        auto& scenar = stateMachine.model();
//        // If we came here through a state.
//        auto evId = this->clickedEvent;
//        if (!bool(evId) && bool(this->clickedState))
//        {
//          evId = scenar.state(*this->clickedState).eventId();
//        }
//        if (!evId)
//          return;

//        auto tnId = scenar.event(*evId).timeSync();

//        if (this->currentPoint.date <= this->m_clickedPoint.date)
//        {
//            auto& tn = scenar.timeSync(tnId);
//            const auto& prev = Scenario::previousIntervals(tn, scenar);
//            if(!prev.empty())
//                return;
//        }

//        if (this->hoveredTimeSync && tnId != this->hoveredTimeSync)
//        {
//          this->m_movingDispatcher.rollback();
//          this->m_mergingEventDispatcher.rollback();

//          this->m_mergingTnDispatcher.submit(
//              this->m_scenario,
//              tnId,
//              *this->hoveredTimeSync);
//        }
//        else
//        {
//          qDebug("stuck 1");
//        }

//      });
//      QObject::connect(rollbackTnMerging, &QState::entered, [&]() {
//          qDebug("rollbackTnMerging");
//        m_mergingTnDispatcher.rollback();
//      });

//      QObject::connect(mergingOnEvent, &QState::entered, [&]() {
//          qDebug("mergingOnEvent");

//        auto& scenar = stateMachine.model();
//        // If we came here through a state.
//        auto clickedEvId = this->clickedEvent;
//        if (!bool(clickedEvId) && bool(this->clickedState))
//        {
//          clickedEvId = scenar.state(*this->clickedState).eventId();
//          qDebug("event 1");
//        }
//        else
//        {
//            qDebug("event 2")
//        }
//        if (!clickedEvId) { qDebug("bye 1");
//          return;
//        }

//        auto destinationEvId = this->hoveredEvent;
//        if (!bool(destinationEvId) && bool(this->hoveredState))
//        {
//          destinationEvId = scenar.state(*this->hoveredState).eventId();
//        }

//        if (!destinationEvId) { qDebug("bye 2");
//          return;
//        }
//        auto tnId = scenar.event(*destinationEvId).timeSync();

//        if (this->currentPoint.date <= this->m_clickedPoint.date)
//        {
//            auto& tn = scenar.timeSync(tnId);
//            const auto& prev = Scenario::previousIntervals(tn, scenar);
//            if(!prev.empty()) { qDebug("bye 3");
//                return;
//            }
//        }

//        qDebug() << *clickedEvId << *destinationEvId;
//        if (*clickedEvId != *destinationEvId)
//        {
//          m_movingDispatcher.rollback();
//          m_mergingTnDispatcher.rollback();

//          m_mergingEventDispatcher.submit(
//              m_scenario,
//              *clickedEvId,
//              *destinationEvId);
//        }
//        else
//        {
//          m_mergingEventDispatcher.rollback();
//          m_mergingTnDispatcher.rollback();

//          TimeVal date
//              = this->m_pressedPrevious
//                    ? max(this->currentPoint.date, *this->m_pressedPrevious)
//                    : this->currentPoint.date;

//          if(this->clickedState)
//          {
//              this->m_movingDispatcher.submit(
//                  m_scenario,
//                  *clickedEvId,
//                  date,
//                  this->currentPoint.y,
//                  stateMachine.editionSettings().expandMode(),
//                  *this->clickedState);
//          }
//          else
//          {
//              this->m_movingDispatcher.submit(
//                  m_scenario,
//                  *clickedEvId,
//                  date,
//                  this->currentPoint.y,
//                  stateMachine.editionSettings().expandMode());
//          }
//        }
//      });
//      QObject::connect(rollbackEventMerging, &QState::entered, [&]() {
//          qDebug("rollbackEventMerging");
//        m_mergingEventDispatcher.rollback();
//      });

//      QObject::connect(onlyMoving, &QState::entered, [&]() {
//          qDebug("onlyMoving");
//        auto& scenar = stateMachine.model();
//        // If we came here through a state.
//        auto evId = this->clickedEvent;
//        if (!bool(evId) && bool(this->clickedState))
//        {
//          evId = scenar.state(*this->clickedState).eventId();
//        }
//        if (!evId)
//          return;

//        TimeVal date
//            = this->m_pressedPrevious
//                  ? std::max(this->currentPoint.date,
//                  *this->m_pressedPrevious) : this->currentPoint.date;

//        date = std::max(date, TimeVal{});
//        if(this->clickedState)
//        {
//            this->m_movingDispatcher.submit(
//                m_scenario,
//                *evId,
//                date,
//                this->currentPoint.y,
//                stateMachine.editionSettings().expandMode(),
//                        *this->clickedState);
//        }
//        else
//        {
//            this->m_movingDispatcher.submit(
//                m_scenario,
//                *evId,
//                date,
//                this->currentPoint.y,
//                stateMachine.editionSettings().expandMode());
//        }
//      });

//      QObject::connect(pressed, &QState::entered, [&]() {
//        this->m_clickedPoint = this->currentPoint;

//        const Scenario::ProcessModel& scenar = stateMachine.model();

//        // TODO refactor this part, it's used everywhere
//        auto evId = this->clickedEvent;
//        if (!bool(evId) && bool(this->clickedState))
//        {
//          evId = scenar.state(*this->clickedState).eventId();
//        }
//        if (!evId)
//          return;

//        auto prev_csts = Scenario::previousIntervals(scenar.event(*evId),
//        scenar); if (!prev_csts.empty())
//        {
//          // We find the one that starts the latest.
//          TimeVal t = TimeVal::zero();
//          for (const auto& cst_id : prev_csts)
//          {
//            const auto& other_date = scenar.interval(cst_id).date();
//            if (other_date > t)
//              t = other_date;
//          }

//          // These 10 milliseconds are here to prevent "squashing"
//          // processes to zero, which leads to problem (they can't scale
//          back!) this->m_pressedPrevious = t + TimeVal::fromMsecs(10);
//        }
//        else
//        {
//          this->m_pressedPrevious = std::nullopt;
//        }

//      });

//      QObject::connect(released, &QState::entered, [&]() {
//        m_movingDispatcher.commit();
//        m_mergingEventDispatcher.commit();
//        m_mergingTnDispatcher.commit();
//        m_pressedPrevious = std::nullopt;
//      });
//    }

//    auto rollbackState = new QState{this};
//    score::make_transition<score::Cancel_Transition>(
//        mainState, rollbackState);
//    rollbackState->addTransition(finalState);
//    QObject::connect(rollbackState, &QState::entered, [&]() {
//      m_movingDispatcher.rollback();
//      m_mergingTnDispatcher.rollback();
//      m_mergingEventDispatcher.rollback();
//    });

//    this->setInitialState(mainState);
//  }

//  SingleOngoingCommandDispatcher<MoveEventCommand_T> m_movingDispatcher;
//  SingleOngoingCommandDispatcher<Command::MergeTimeSyncs>
//  m_mergingTnDispatcher; SingleOngoingCommandDispatcher<Command::MergeEvents>
//  m_mergingEventDispatcher;

//  std::optional<TimeVal> m_pressedPrevious;
//  Scenario::Point m_clickedPoint;
//};
}
