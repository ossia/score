// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "DefaultClock.hpp"

#include <Execution/BaseScenarioComponent.hpp>
#include <Execution/Settings/ExecutorModel.hpp>
#include <Process/ExecutionContext.hpp>

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/graph/graph_interface.hpp>
#include <ossia/editor/scenario/execution_log.hpp>

#include <QDebug>

#include <Scenario/Document/Event/EventExecution.hpp>
#include <Scenario/Document/Interval/IntervalExecution.hpp>
#include <Scenario/Execution/score2OSSIA.hpp>

namespace Execution
{
DefaultClock::~DefaultClock() = default;
DefaultClock::DefaultClock(const Context& ctx)
    : context{ctx}
{
}

void DefaultClock::prepareExecution(const TimeVal& t, BaseScenarioElement& bs)
{
  using namespace ossia;
  auto& settings = context.doc.app.settings<Execution::Settings::Model>();
  IntervalComponentBase& comp = bs.baseInterval();
  const auto& oc = comp.OSSIAInterval();
  if (settings.getValueCompilation())
  {
    comp.interval().duration.setPlayPercentage(0);

#if defined(OSSIA_EXECUTION_LOG)
    auto log = ossia::g_exec_log.init();
#endif
    context.executionQueue.enqueue([time = context.time(t),
                                    oc,
                                    g = context.execGraph,
                                    s = context.execState] {
      if (time != 0_tv)
        oc->offset(time);

      s->commit();
    });
  }
  else
  {
    context.executionQueue.enqueue([time = context.time(t), oc] {
      if (time != 0_tv)
        oc->transport(time);
    });
  }
}

void DefaultClock::play(const TimeVal& t, BaseScenarioElement& bs)
{
  prepareExecution(t, bs);
  try
  {
#if defined(OSSIA_EXECUTION_LOG)
    auto log = ossia::g_exec_log.init();
#endif
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


}
