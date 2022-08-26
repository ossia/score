#include <Audio/AudioTick.hpp>

namespace Audio
{
SCORE_PLUGIN_AUDIO_EXPORT
std::atomic<ossia::transport_status> execution_status;
tick_fun makePauseTick(const score::ApplicationContext& app)
{
  std::vector<Execution::ExecutionAction*> actions;
  for(Execution::ExecutionAction& act : app.interfaces<Execution::ExecutionActionList>())
  {
    actions.push_back(&act);
  }

  return [actions = std::move(actions)](const ossia::audio_tick_state& t) mutable {
    execution_status.store(ossia::transport_status::stopped);

    for(int chan = 0; chan < t.n_out; chan++)
    {
      float* c = t.outputs[chan];
      for(std::size_t i = 0; i < t.frames; i++)
      {
        c[i] = 0.f;
      }
    }

    try
    {
      for(auto act : actions)
        act->startTick(t);
      for(auto act : actions)
        act->endTick(t);
    }
    catch(...)
    {
    }
  };
}
}
