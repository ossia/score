// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <ossia/editor/state/state_element.hpp>
#include "DefaultClockManager.hpp"

#include <ossia/editor/state/state_element.hpp>
#include <Engine/Executor/BaseScenarioComponent.hpp>
#include <Engine/Executor/IntervalComponent.hpp>
#include <Engine/Executor/EventComponent.hpp>
#include <Engine/Executor/DocumentPlugin.hpp>
#include <Engine/Executor/ExecutorContext.hpp>
#include <Engine/Executor/Settings/ExecutorModel.hpp>
#include <Engine/OSSIA2score.hpp>
#include <Engine/score2OSSIA.hpp>

#include <Scenario/Document/Interval/IntervalDurations.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <ossia/editor/scenario/clock.hpp>
#include <ossia/editor/state/state_element.hpp>
#include <ossia/editor/scenario/time_event.hpp>
#include <ossia/dataflow/graph.hpp>

namespace Engine
{
namespace Execution
{
DefaultClockManager::~DefaultClockManager() = default;
DefaultClockManager::DefaultClockManager(const Context& ctx)
    : ClockManager{ctx}
{
  auto& bs = ctx.scenario;
  ossia::time_interval& ossia_cst = *bs.baseInterval().OSSIAInterval();
  ossia_cst.set_callback(makeDefaultCallback(bs));
}
ossia::time_interval::exec_callback
DefaultClockManager::makeDefaultCallback(
    Engine::Execution::BaseScenarioElement& bs)
{
  auto& cst = bs.baseInterval();
  auto& ctx = this->context;
  return smallfun::SmallFun<void(double, ossia::time_value), 32>{[&ctx, &score_cst=cst.scoreInterval()](
      double position,
      ossia::time_value date)
  {
    ctx.editionQueue.enqueue([&score_cst,currentTime = ctx.reverseTime(date)] {
      auto& cstdur = score_cst.duration;
      const auto& maxdur = cstdur.maxDuration();

      if (!maxdur.isInfinite())
        cstdur.setPlayPercentage(currentTime / cstdur.maxDuration());
      else
        cstdur.setPlayPercentage(currentTime / cstdur.defaultDuration());
    });


    // Run some commands if they have been submitted.
    ExecutionCommand c;
    while(ctx.executionQueue.try_dequeue(c))
    {
      c();
    }
  }};
}

void DefaultClockManager::prepareExecution(
    const TimeVal& t, BaseScenarioElement& bs)
{
  IntervalComponentBase& comp = bs.baseInterval();
  comp.interval().duration.setPlayPercentage(0);

  // Send the first state
  const auto& oc = comp.OSSIAInterval();
  oc->get_start_event().tick(ossia::Zero);
  context.plugin.execGraph->state(context.plugin.execState);

  oc->offset(context.time(t));

  context.plugin.execState.commit();
}

void DefaultClockManager::play_impl(
    const TimeVal& t, BaseScenarioElement& bs)
{
  prepareExecution(t, bs);
  try
  {
    bs.baseInterval().OSSIAInterval()->start_and_tick();
    bs.baseInterval().executionStarted();
  }
  catch (const std::exception& e)
  {
    qDebug() << e.what();
  }
}

void DefaultClockManager::pause_impl(BaseScenarioElement& bs)
{
  bs.baseInterval().pause();
}

void DefaultClockManager::resume_impl(BaseScenarioElement& bs)
{
  bs.baseInterval().resume();
}

void DefaultClockManager::stop_impl(BaseScenarioElement& bs)
{
  bs.baseInterval().stop();
}

ControlClockFactory::~ControlClockFactory() = default;




ControlClock::ControlClock(
        const Engine::Execution::Context& ctx):
    ClockManager{ctx},
    m_default{ctx},
    m_clock{*ctx.scenario.baseInterval().OSSIAInterval(), 1.}
{
  m_clock.set_granularity(std::chrono::microseconds(
      context.doc.app.settings<Settings::Model>().getRate() * 1000));

  // TODO this should be the case also with other clocks
  m_clock.set_exec_status_callback(
      [=](ossia::clock::exec_status c) {
        if (c == ossia::clock::exec_status::STOPPED)
        {
          context.scenario.endEvent().OSSIAEvent()->tick(ossia::Zero);
          context.plugin.execGraph->state(context.plugin.execState);
          context.plugin.execState.commit();

          emit context.scenario.finished();
        }
      });
}

void ControlClock::play_impl(
        const TimeVal& t,
        Engine::Execution::BaseScenarioElement& bs)
{
    m_default.prepareExecution(t, bs);
    try
    {
      m_clock.start_and_tick();
      bs.baseInterval().executionStarted();
    }
    catch (const std::exception& e)
    {
      qDebug() << e.what();
    }
}

void ControlClock::pause_impl(
        Engine::Execution::BaseScenarioElement& bs)
{
    m_clock.pause();
    m_default.pause();
}

void ControlClock::resume_impl(
        Engine::Execution::BaseScenarioElement& bs)
{
    m_default.resume();
    m_clock.resume();
}

void ControlClock::stop_impl(
        Engine::Execution::BaseScenarioElement& bs)
{
    m_clock.stop();
    m_default.stop();
}

bool ControlClock::paused() const
{
    return m_clock.paused();
}


QString ControlClockFactory::prettyName() const
{
  return QObject::tr("Default");
}

std::unique_ptr<ClockManager>
ControlClockFactory::make(const Engine::Execution::Context& ctx)
{
  return std::make_unique<ControlClock>(ctx);
}

Engine::Execution::time_function
ControlClockFactory::makeTimeFunction(const score::DocumentContext& ctx) const
{
  return &score_to_ossia::defaultTime;
}

Engine::Execution::reverse_time_function ControlClockFactory::makeReverseTimeFunction(const score::DocumentContext& ctx) const
{
  return &ossia_to_score::defaultTime;
}
}
}
