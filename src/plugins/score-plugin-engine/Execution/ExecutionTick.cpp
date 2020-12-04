#include <Audio/AudioTick.hpp>

#include <ossia/dataflow/graph/graph_interface.hpp>
#include <ossia/audio/audio_protocol.hpp>

#include <Execution/BaseScenarioComponent.hpp>
#include <Execution/DocumentPlugin.hpp>
#include <Scenario/Document/Interval/IntervalExecution.hpp>
#include <ossia/editor/scenario/execution_log.hpp>

namespace Execution
{
namespace
{
struct AudioTickHelper
{
#if defined(OSSIA_EXECUTION_LOG)
  std::shared_ptr<ossia::on_destruct> log_context = std::make_shared<ossia::on_destruct>(ossia::g_exec_log.init());
#endif
  AudioTickHelper(
      ossia::tick_setup_options opt,
      Execution::DocumentPlugin& plug,
      Execution::BaseScenarioElement& scenar)
    : m_tick{ossia::make_tick(opt, *plug.execState, *plug.execGraph, *scenar.baseInterval().OSSIAInterval())}
    , m_plug{plug}
    , m_proto{plug.audioProto()}
  {
    m_actions = plug.actions();
    for (Execution::ExecutionAction& act : plug.context().doc.app.interfaces<Execution::ExecutionActionList>())
    {
      m_actions.push_back(&act);
    }
  }

  void clearBuffers(const ossia::audio_tick_state& t) const
  {
    // Clear buffers as some APIs are nastyyyy
    for(int chan = 0; chan < t.n_out; chan++)
    {
      float* c = t.outputs[chan];
      for(std::size_t i = 0; i < t.frames; i++)
      {
        c[i] = 0.f;
      }
    }
  }

  void dequeueCommands() const
  try
  {
    // Run some commands if they have been submitted.
    Execution::ExecutionCommand c;
    auto& exec = m_plug.context().executionQueue;
    auto& gcq = m_plug.context().gcQueue;
    while (exec.try_dequeue(c))
    {
      c();
      gcq.enqueue(gc(std::move(c)));
    }
  } catch (...) { }

  void main(const ossia::audio_tick_state& t) const
  try
  {
    // Match the audio_protocol with the actual I/O
    m_proto.setup_buffers(t);

    // The actual tick
    for (auto act : m_actions)
      act->startTick(t);

    m_tick(t.frames, t.seconds);

    for (auto act : m_actions)
      act->endTick(t);
  } catch (...) { }

  smallfun::function<void(unsigned long, double), 128> m_tick;
  Execution::DocumentPlugin& m_plug;
  ossia::audio_protocol& m_proto;
  std::vector<ExecutionAction*> m_actions;
};
}


Audio::tick_fun makeExecutionTick(
    ossia::tick_setup_options opt,
    Execution::DocumentPlugin& plug,
    Execution::BaseScenarioElement& scenar)
{
  return [helper = AudioTickHelper{opt, plug, scenar}] (ossia::audio_tick_state t)
  {
    helper.clearBuffers(t);
      helper.dequeueCommands();
      helper.main(t);
  };
}

Audio::tick_fun makeBenchmarkTick(
    ossia::tick_setup_options opt,
    Execution::DocumentPlugin& plug,
    Execution::BaseScenarioElement& scenar)
{
  auto tick = ossia::make_tick(
      opt, *plug.execState, *plug.execGraph, *scenar.baseInterval().OSSIAInterval());

  int i = 0;
  return [helper = AudioTickHelper{opt, plug, scenar}, i]
      (ossia::audio_tick_state t) mutable
  {
    helper.clearBuffers(t);
    helper.dequeueCommands();

    auto& bench = *helper.m_plug.bench;
    if (i % 50 == 0)
    {
      bench.measure = true;
      auto t0 = std::chrono::steady_clock::now();

      helper.main(t);

      auto t1 = std::chrono::steady_clock::now();
      auto total = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();

      helper.m_plug.slot_bench(bench, total);
      for (auto& p : bench)
      {
        p.second = {};
      }
    }
    else
    {
      bench.measure = false;

      helper.main(t);
    }

    i++;
  };
}
}
