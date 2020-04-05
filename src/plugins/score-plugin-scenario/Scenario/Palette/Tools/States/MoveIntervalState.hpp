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

#include <Scenario/Document/Event/EventView.hpp>
#include <Scenario/Document/Event/EventPresenter.hpp>
#include <Scenario/Document/State/StateView.hpp>
#include <Scenario/Document/State/StatePresenter.hpp>
#include <Scenario/Document/TimeSync/TimeSyncView.hpp>
#include <Scenario/Document/TimeSync/TimeSyncPresenter.hpp>
#include <Scenario/Document/TimeSync/TriggerView.hpp>
#include <Scenario/Document/Interval/Temporal/TemporalIntervalPresenter.hpp>
#include <Scenario/Document/Interval/Temporal/TemporalIntervalView.hpp>
#include <score/command/Dispatchers/MultiOngoingCommandDispatcher.hpp>
#include <score/locking/ObjectLocker.hpp>
#include <Scenario/Commands/Scenario/Merge/MergeEvents.hpp>

#include <QApplication>
#include <QFinalState>

namespace Scenario
{
namespace Command
{
class MoveIntervalMacro final : public score::AggregateCommand
{
  SCORE_COMMAND_DECL(
      CommandFactoryName(),
      MoveIntervalMacro,
      "Move an interval")
};
}
}

namespace Scenario
{
template <typename T>
class MoveIntervalState final : public StateBase<Scenario::ProcessModel>
{
  const T& m_sm;
public:
  MoveIntervalState(
      const T& stateMachine,
      const Scenario::ProcessModel& scenario,
      const score::CommandStackFacade& stack,
      score::ObjectLocker& locker,
      QState* parent)
      : StateBase<Scenario::ProcessModel>{scenario, parent}
      , m_sm{stateMachine}
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
        auto& eev = Scenario::endEvent(cst, scenario);

        m_intervalInitialPoint = {cst.date(), cst.heightPercentage()};
        m_lastDate = m_intervalInitialPoint.date;

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

        this->m_startEventCanBeMerged = previousIntervals(sev, scenar).empty();
        this->m_endEventCanBeMerged = nextIntervals(eev, scenar).empty();
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
        auto& sst = Scenario::startState(cst, scenario);
        auto& sev = Scenario::parentEvent(sst, scenario);

        if (qApp->keyboardModifiers() & Qt::ShiftModifier)
          m_lastDate = m_intervalInitialPoint.date
                 + (this->currentPoint.date - m_initialClick.date);
        else
          m_lastDate = m_intervalInitialPoint.date;
        if (this->m_pressedPrevious)
          m_lastDate = std::max(m_lastDate, *this->m_pressedPrevious);

        m_lastDate = stateMachine.magnetic().getPosition(&stateMachine.model(), m_lastDate);
        m_lastDate = std::max(m_lastDate, TimeVal{});

        // If the start event does not have previous intervals
        // we will try to merge the start sync with other syncs...
        // Idea : if it's an empty interval we can maybe merge...
        // and if we are dragging an interval we can maybe create an empty interval between...
        /// we have to check for it during the press, not the move

        this->m_movingDispatcher.template submit<Command::MoveEventMeta>(
              this->m_scenario,
              sev.id(),
              m_lastDate,
              m_intervalInitialPoint.y
              + (this->currentPoint.y - m_initialClick.y),
              stateMachine.editionSettings().expandMode(),
              stateMachine.editionSettings().lockMode(),
              cst.startState());
      });

      QObject::connect(released, &QState::entered, [&]() {

        auto& cst = m_scenario.interval(*this->clickedInterval);
        if(this->m_startEventCanBeMerged)
        {
          merge(cst, Scenario::startState(cst, m_scenario), m_lastDate);
        }
        if(this->m_endEventCanBeMerged)
        {
          merge(cst, Scenario::endState(cst, m_scenario), m_lastDate + cst.duration.defaultDuration());
        }

        m_movingDispatcher.template commit<Command::MoveIntervalMacro>();
        m_pressedPrevious = {};
      });
    }

    auto rollbackState = new QState{this};
    score::make_transition<score::Cancel_Transition>(mainState, rollbackState);
    rollbackState->addTransition(finalState);
    QObject::connect(rollbackState, &QState::entered, [&]() {
      this->rollback();
      m_pressedPrevious = {};
    });

    this->setInitialState(mainState);
  }

  // SingleOngoingCommandDispatcher<MoveIntervalCommand_T> m_dispatcher;
  MultiOngoingCommandDispatcher m_movingDispatcher;

  void rollback()
  {
    m_movingDispatcher.template rollback<DefaultRollbackStrategy>();
  }

  void merge(const IntervalModel& cst, const StateModel& st, TimeVal date)
  {
    auto& ev = Scenario::parentEvent(st, m_scenario);
    auto& ts = Scenario::parentTimeSync(ev, m_scenario);

    auto& sst_pres = m_sm.presenter().state(st.id());
    auto& sev_pres = m_sm.presenter().event(ev.id());
    auto& sts_pres = m_sm.presenter().timeSync(ts.id());
    auto& itv_pres = m_sm.presenter().interval(cst.id());

    std::vector<QGraphicsItem*> toIgnore;
    toIgnore.push_back(sst_pres.view());
    toIgnore.push_back(sev_pres.view());
    toIgnore.push_back(sts_pres.view());
    toIgnore.push_back(&sts_pres.trigger());
    toIgnore.push_back(itv_pres.view());
    QGraphicsItem* item = m_sm.itemAt({date, cst.heightPercentage()}, toIgnore);

    if(auto stateToMerge = qgraphicsitem_cast<Scenario::StateView*>(item))
    {
      // this->rollback();
      this->m_movingDispatcher.template submit<Command::MergeEvents>(
            this->m_scenario,
            ev.id(),
            Scenario::parentEvent(stateToMerge->presenter().model().id(), this->m_scenario).id());
    }
    else if(auto eventToMerge = qgraphicsitem_cast<Scenario::EventView*>(item))
    {
      // this->rollback();
      this->m_movingDispatcher.template submit<Command::MergeEvents>(
            this->m_scenario,
            ev.id(),
            eventToMerge->presenter().model().id());
    }
    else if(auto syncToMerge = qgraphicsitem_cast<Scenario::TimeSyncView*>(item))
    {
      // this->rollback();
      this->m_movingDispatcher.template submit<Command::MergeTimeSyncs>(
            this->m_scenario,
            ts.id(),
            syncToMerge->presenter().model().id());
    }
  }
private:
  Scenario::Point m_initialClick{};
  Scenario::Point m_intervalInitialPoint{};
  optional<TimeVal> m_pressedPrevious;
  TimeVal m_lastDate{};
  bool m_startEventCanBeMerged{};
  bool m_endEventCanBeMerged{};
};

}
