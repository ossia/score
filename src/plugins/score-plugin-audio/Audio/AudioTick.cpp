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

  return [actions = std::move(actions)](unsigned long samples, double sec) {
    for (auto act : actions)
      act->startTick(samples, sec);
    for (auto act : actions)
      act->endTick(samples, sec);
  };
}

}
