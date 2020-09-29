#include <Audio/AudioTick.hpp>


namespace Audio
{
tick_fun makePauseTick(const score::ApplicationContext& app)
{
  std::vector<Execution::ExecutionAction*> actions;
  for (Execution::ExecutionAction& act : app.interfaces<Execution::ExecutionActionList>())
  {
    actions.push_back(&act);
  }

  return [actions = std::move(actions)] (ossia::audio_tick_state t) {
    for (auto act : actions)
      act->startTick(t);
    for (auto act : actions)
      act->endTick(t);
  };
}

}
