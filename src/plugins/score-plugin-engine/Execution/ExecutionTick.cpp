#include <Process/ExecutionCommand.hpp>

#include <Scenario/Document/Interval/IntervalExecution.hpp>

#include <Audio/AudioTick.hpp>
#include <Audio/Settings/Model.hpp>
#include <Execution/BaseScenarioComponent.hpp>
#include <Execution/DocumentPlugin.hpp>
#include <Execution/ExecutionController.hpp>

#include <ossia/audio/audio_protocol.hpp>
#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/graph/graph_interface.hpp>
#include <ossia/dataflow/graph/tick_setup.hpp>
#include <ossia/editor/scenario/execution_log.hpp>
#include <ossia/editor/scenario/scenario.hpp>
#include <ossia/editor/scenario/time_interval.hpp>

#include <Transport/TransportInterface.hpp>
namespace Execution
{

namespace
{
struct AudioTickHelper
{
#if defined(OSSIA_EXECUTION_LOG)
  std::shared_ptr<ossia::on_destruct> log_context
      = std::make_shared<ossia::on_destruct>(ossia::g_exec_log.init());
#endif
  AudioTickHelper(
      ossia::tick_setup_options opt, Execution::DocumentPlugin& plug,
      const std::shared_ptr<Execution::BaseScenarioElement>& scenar)
      : m_scenar{scenar}
      , m_context{plug.contextData()}
      , m_itv{*scenar->baseInterval().OSSIAInterval()}
      , m_proto{plug.audioProto()}
      , m_actions{plug.actions()}
  {
    m_tick = ossia::make_tick(
        opt, *m_context->execState, *m_context->execGraph, m_itv, scenar->baseScenario(),
        plug.executionController().transport().transportUpdateFunction());

    for(Execution::ExecutionAction& act :
        plug.context().doc.app.interfaces<Execution::ExecutionActionList>())
    {
      m_actions.push_back(&act);
    }
  }

  void runAllCommands() const
  {
    std::atomic_thread_fence(std::memory_order_seq_cst);
    ExecutionCommand com;
    while(m_context->m_execQueue.try_dequeue(com))
      com();
  }
  ~AudioTickHelper()
  {
    runAllCommands();
    runAllCommands();
    m_scenar->cleanup();
    runAllCommands();
    runAllCommands();
    m_context->execGraph->clear();
    runAllCommands();
    runAllCommands();
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
  {
    // Run some commands if they have been submitted.
    Execution::ExecutionCommand c;
    while(m_context->m_execQueue.try_dequeue(c))
    {
      try
      {
        c();
        m_context->m_gcQueue.enqueue(gc(std::move(c)));
      }
      catch(...)
      {
      }
    }
  }

  void main_tick(const ossia::audio_tick_state& t) const
  {
    // TODO this means that transport isn't visible until we play again
    if(t.status && *t.status != ossia::transport_status::playing)
      return;

    // Transport if necessary
    if(t.position_in_frames)
    {
      auto cur = *t.position_in_frames;
      if(m_prev_frame)
      {
        auto prev = *m_prev_frame;
        if(prev + t.frames == cur)
        {
          // Normal playback
          m_tick(t.frames, t.seconds);
        }
        else if(prev == cur)
        {
          // Pause, do not advance the score execution
        }
        else
        {
          // transport
          // TODO here we must multiply by root_tempo / tempo  ... if we have  a fixed tempo !
          // If we have a tempo map we have to integrate over it to compute which logical duration
          // maps to which physical duration.
          const auto flicks = cur * m_context->execState->samplesToModelRatio;
          m_itv.transport(ossia::time_value{int64_t(flicks)});

          m_tick(t.frames, t.seconds);
        }
      }
      else
      {
        // first time this is called, m_prev_frame not initialized yet
        if(cur != 0)
        {
          // transport to pur ourselves aligned with the global clock
          const auto flicks = cur * m_context->execState->samplesToModelRatio;
          m_itv.transport(ossia::time_value{int64_t(flicks)});
        }

        m_tick(t.frames, t.seconds);
      }
      m_prev_frame = *t.position_in_frames;
    }
    else
    {
      // No transport information, normal playback
      m_tick(t.frames, t.seconds);
    }
  }

  void main(const ossia::audio_tick_state& t) const
  try
  {
    // Match the audio_protocol with the actual I/O
    m_proto->setup_buffers(t);

    // The actual tick
    for(auto act : m_actions)
      act->startTick(t);

    main_tick(t);

    for(auto act : m_actions)
      act->endTick(t);
  }
  catch(...)
  {
  }

  std::shared_ptr<Execution::BaseScenarioElement> m_scenar;
  std::shared_ptr<Execution::DocumentPlugin::ContextData> m_context;

  ossia::time_interval& m_itv;
  smallfun::function<void(unsigned long, double), 128> m_tick;
  std::shared_ptr<ossia::audio_protocol> m_proto;
  std::vector<ExecutionAction*> m_actions;

  mutable std::optional<uint64_t> m_prev_frame;
};
}

Audio::tick_fun makeExecutionTick(
    ossia::tick_setup_options opt, Execution::DocumentPlugin& plug,
    const std::shared_ptr<Execution::BaseScenarioElement>& scenar)
{
  return [helper = std::make_shared<AudioTickHelper>(opt, plug, scenar)](
             const ossia::audio_tick_state& t) {
    Audio::execution_status.store(ossia::transport_status::playing);

    helper->clearBuffers(t);
    helper->dequeueCommands();
    helper->main(t);
  };
}

Audio::tick_fun makeBenchmarkTick(
    ossia::tick_setup_options opt, Execution::DocumentPlugin& plug,
    const std::shared_ptr<Execution::BaseScenarioElement>& scenar)
{
  int i = 0;
  QPointer<Execution::DocumentPlugin> plugPtr = &plug;
  return [helper = std::make_shared<AudioTickHelper>(opt, plug, scenar), plugPtr,
          i](const ossia::audio_tick_state& t) mutable {
    Audio::execution_status.store(ossia::transport_status::playing);

    helper->clearBuffers(t);
    helper->dequeueCommands();

    auto& bench = *helper->m_context->bench;
    if(i % 50 == 0)
    {
      bench.measure = true;
      auto t0 = std::chrono::steady_clock::now();

      helper->main(t);

      auto t1 = std::chrono::steady_clock::now();
      auto total = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
#if !defined(_MSC_VER)
      //FIXME: MSVC unordered_map isn't move noexcept so it does not work here in Debug
      helper->m_context->m_editionQueue.enqueue([plugPtr, bench, total]() mutable {
        if(plugPtr)
          plugPtr->slot_bench(std::move(bench), total);
      });
#endif

      for(auto& p : bench)
      {
        p.second = {};
      }
    }
    else
    {
      bench.measure = false;

      helper->main(t);
    }

    i++;
  };
}
}
