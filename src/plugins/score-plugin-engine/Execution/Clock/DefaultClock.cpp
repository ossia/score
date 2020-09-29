// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "DefaultClock.hpp"

#include <Process/ExecutionContext.hpp>
#include <Scenario/Document/Event/EventExecution.hpp>
#include <Scenario/Document/Interval/IntervalExecution.hpp>
#include <Scenario/Execution/score2OSSIA.hpp>

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/graph/graph_interface.hpp>

#include <QDebug>

#include <Execution/BaseScenarioComponent.hpp>
#include <Execution/Settings/ExecutorModel.hpp>

namespace Execution
{
DefaultClock::~DefaultClock() = default;
DefaultClock::DefaultClock(const Context& ctx) : context{ctx} { }

void DefaultClock::prepareExecution(const TimeVal& t, BaseScenarioElement& bs)
{
  auto& settings = context.doc.app.settings<Execution::Settings::Model>();
  IntervalComponentBase& comp = bs.baseInterval();
  if (settings.getValueCompilation())
  {
    comp.interval().duration.setPlayPercentage(0);

    // Send the first state
    const auto& oc = comp.OSSIAInterval();
    oc->get_start_event().tick(ossia::Zero, ossia::Zero);
    context.execGraph->state(*context.execState);

    if (t != TimeVal::zero())
      oc->offset(context.time(t));

    context.execState->commit();
  }
}

void DefaultClock::play(const TimeVal& t, BaseScenarioElement& bs)
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

void DefaultClock::pause(BaseScenarioElement& bs)
{
  bs.baseInterval().pause();
}

void DefaultClock::resume(BaseScenarioElement& bs)
{
  bs.baseInterval().resume();
}

void DefaultClock::stop(BaseScenarioElement& bs)
{
  bs.baseInterval().stop();
}

ControlClockFactory::~ControlClockFactory() = default;

ControlClock::ControlClock(const Execution::Context& ctx)
    : Clock{ctx}, m_default{ctx}, m_clock{*scenario.baseInterval().OSSIAInterval(), 1.}
{
  m_clock.set_granularity(
      std::chrono::microseconds(context.doc.app.settings<Settings::Model>().getRate() * 1000));

  // TODO this should be the case also with other clocks
  m_clock.set_exec_status_callback([=](ossia::clock::exec_status c) {
    if (c == ossia::clock::exec_status::STOPPED)
    {
      scenario.endEvent().OSSIAEvent()->tick(ossia::Zero, ossia::Zero);
      context.execGraph->state(*context.execState);
      context.execState->commit();

      scenario.finished();
    }
  });
}

void ControlClock::play_impl(const TimeVal& t, Execution::BaseScenarioElement& bs)
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

void ControlClock::pause_impl(Execution::BaseScenarioElement& bs)
{
  m_clock.pause();
  m_default.pause(bs);
}

void ControlClock::resume_impl(Execution::BaseScenarioElement& bs)
{
  m_default.resume(bs);
  m_clock.resume();
}

void ControlClock::stop_impl(Execution::BaseScenarioElement& bs)
{
  m_default.stop(bs);
  m_clock.stop();
}

bool ControlClock::paused() const
{
  return m_clock.paused();
}

QString ControlClockFactory::prettyName() const
{
  return QObject::tr("Default");
}

std::unique_ptr<Clock> ControlClockFactory::make(const Execution::Context& ctx)
{
  return std::make_unique<ControlClock>(ctx);
}

Execution::time_function
ControlClockFactory::makeTimeFunction(const score::DocumentContext& ctx) const
{
  SCORE_ABORT;
  return {};
  // return &Engine::score_to_ossia::defaultTime;
}

Execution::reverse_time_function
ControlClockFactory::makeReverseTimeFunction(const score::DocumentContext& ctx) const
{
  SCORE_ABORT;
  return {};
  // return &Engine::ossia_to_score::defaultTime;
}
}
