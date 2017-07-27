// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <ossia/editor/state/state_element.hpp>
#include "DefaultClockManager.hpp"

#include <ossia/editor/state/state_element.hpp>
#include <Engine/Executor/BaseScenarioComponent.hpp>
#include <Engine/Executor/ConstraintComponent.hpp>
#include <Engine/Executor/EventComponent.hpp>
#include <Engine/Executor/DocumentPlugin.hpp>
#include <Engine/Executor/ExecutorContext.hpp>
#include <Engine/Executor/Settings/ExecutorModel.hpp>
#include <Engine/OSSIA2iscore.hpp>
#include <Engine/iscore2OSSIA.hpp>

#include <Scenario/Document/Constraint/ConstraintDurations.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>

#include <ossia/editor/scenario/clock.hpp>
#include <ossia/editor/state/state_element.hpp>
#include <ossia/editor/scenario/time_event.hpp>

namespace Engine
{
namespace Execution
{
DefaultClockManager::~DefaultClockManager() = default;
DefaultClockManager::DefaultClockManager(const Context& ctx)
    : ClockManager{ctx}
{
  auto& bs = ctx.scenario;
  ossia::time_constraint& ossia_cst = *bs.baseConstraint().OSSIAConstraint();
  ossia_cst.set_callback(makeDefaultCallback(bs));
}
ossia::time_constraint::exec_callback
DefaultClockManager::makeDefaultCallback(
    Engine::Execution::BaseScenarioElement& bs)
{
  auto& cst = bs.baseConstraint();
  return [this, &bs, &iscore_cst = cst.iscoreConstraint()](
      double position,
      ossia::time_value date,
      const ossia::state_element& state)
  {
    ossia::launch(state);

    auto currentTime = this->context.reverseTime(date);

    auto& cstdur = iscore_cst.duration;
    const auto& maxdur = cstdur.maxDuration();

    if (!maxdur.isInfinite())
      cstdur.setPlayPercentage(currentTime / cstdur.maxDuration());
    else
      cstdur.setPlayPercentage(currentTime / cstdur.defaultDuration());

    // Run some commands if they have been submitted.
    ExecutionCommand c;
    while(context.executionQueue.try_dequeue(c))
    {
      c();
    }
  };
}

void DefaultClockManager::prepareExecution(
    const TimeVal& t, BaseScenarioElement& bs)
{
  ConstraintComponentBase& comp = bs.baseConstraint();
  comp.constraint().duration.setPlayPercentage(0);
  const auto& oc = comp.OSSIAConstraint();

  auto start_state = oc->get_start_event().get_state();
  auto offset_state = oc->offset(context.time(t));

  ossia::state accumulator;
  ossia::flatten_and_filter(accumulator, start_state);
  ossia::flatten_and_filter(accumulator, offset_state);
  accumulator.launch();
}

void DefaultClockManager::play_impl(
    const TimeVal& t, BaseScenarioElement& bs)
{
  prepareExecution(t, bs);
  try
  {
    ossia::state st;
    bs.baseConstraint().OSSIAConstraint()->start(st);
    ossia::launch(st);
    bs.baseConstraint().executionStarted();
  }
  catch (const std::exception& e)
  {
    qDebug() << e.what();
  }
}

void DefaultClockManager::pause_impl(BaseScenarioElement& bs)
{
  bs.baseConstraint().pause();
}

void DefaultClockManager::resume_impl(BaseScenarioElement& bs)
{
  bs.baseConstraint().resume();
}

void DefaultClockManager::stop_impl(BaseScenarioElement& bs)
{
  bs.baseConstraint().stop();
}

ControlClockFactory::~ControlClockFactory() = default;




ControlClock::ControlClock(
        const Engine::Execution::Context& ctx):
    ClockManager{ctx},
    m_default{ctx},
    m_clock{*ctx.scenario.baseConstraint().OSSIAConstraint(), 1.}
{
  m_clock.set_granularity(std::chrono::microseconds(
      context.doc.app.settings<Settings::Model>().getRate() * 1000));

  // TODO this should be the case also with other clocks
  m_clock.set_exec_status_callback(
      [=](ossia::clock::exec_status c) {
        if (c == ossia::clock::exec_status::STOPPED)
        {
          ossia::state accumulator;
          ossia::flatten_and_filter(accumulator, context.scenario.endEvent().OSSIAEvent()->get_state());
          accumulator.launch();

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
      ossia::state st;
      m_clock.start(st);
      ossia::launch(st);
      bs.baseConstraint().executionStarted();
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

std::function<ossia::time_value(const TimeVal&)>
ControlClockFactory::makeTimeFunction(const iscore::DocumentContext& ctx) const
{
  return &iscore_to_ossia::defaultTime;
}

std::function<TimeVal (const ossia::time_value&)> ControlClockFactory::makeReverseTimeFunction(const iscore::DocumentContext& ctx) const
{
  return &ossia_to_iscore::defaultTime;
}
}
}
