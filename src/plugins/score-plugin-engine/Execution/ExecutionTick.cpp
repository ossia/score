#include <Audio/AudioTick.hpp>

#include <ossia/dataflow/graph/graph_interface.hpp>

#include <Execution/BaseScenarioComponent.hpp>
#include <Execution/DocumentPlugin.hpp>
#include <Scenario/Document/Interval/IntervalExecution.hpp>

namespace Execution
{
Audio::tick_fun makeExecutionTick(
    ossia::tick_setup_options opt,
    Execution::DocumentPlugin& m_plug,
    Execution::BaseScenarioElement& m_cur)
{
  auto actions = m_plug.actions();
  for (Execution::ExecutionAction& act : m_plug.context().doc.app.interfaces<Execution::ExecutionActionList>())
  {
    actions.push_back(&act);
  }

  auto tick = ossia::make_tick(
      opt, *m_plug.execState, *m_plug.execGraph, *m_cur.baseInterval().OSSIAInterval());

  return [tick, plug = &m_plug, actions = std::move(actions)] (ossia::audio_tick_state t) {
    // Run some commands if they have been submitted.
    Execution::ExecutionCommand c;
    while (plug->context().executionQueue.try_dequeue(c))
    {
      c();
    }

    for (auto act : actions)
      act->startTick(t);

    tick(t.frames, t.seconds);

    for (auto act : actions)
      act->endTick(t);
  };
}

Audio::tick_fun makeBenchmarkTick(
    ossia::tick_setup_options opt,
    Execution::DocumentPlugin& m_plug,
    Execution::BaseScenarioElement& m_cur)
{
  auto actions = m_plug.actions();
  for (Execution::ExecutionAction& act : m_plug.context().doc.app.interfaces<Execution::ExecutionActionList>())
  {
    actions.push_back(&act);
  }

  auto tick = ossia::make_tick(
      opt, *m_plug.execState, *m_plug.execGraph, *m_cur.baseInterval().OSSIAInterval());

  return [tick, plug = &m_plug, actions = std::move(actions)] (ossia::audio_tick_state t) {
    // Run some commands if they have been submitted.
    Execution::ExecutionCommand c;
    while (plug->context().executionQueue.try_dequeue(c))
    {
      c();
    }

    auto& bench = *plug->bench;
    static int i = 0;
    if (i % 50 == 0)
    {
      bench.measure = true;
      auto t0 = std::chrono::steady_clock::now();

      for (auto act : actions)
        act->startTick(t);

      tick(t.frames, t.seconds);

      for (auto act : actions)
        act->endTick(t);

      auto t1 = std::chrono::steady_clock::now();
      auto total = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();

      plug->slot_bench(bench, total);
      for (auto& p : bench)
      {
        p.second = {};
      }
    }
    else
    {
      bench.measure = false;

      for (auto act : actions)
        act->startTick(t);

      tick(t.frames, t.seconds);

      for (auto act : actions)
        act->endTick(t);
    }

    i++;
  };
}
}
