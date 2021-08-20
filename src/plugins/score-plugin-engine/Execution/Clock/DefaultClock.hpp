#pragma once
#include <Execution/Clock/ClockFactory.hpp>

#include <ossia/editor/scenario/clock.hpp>
#include <ossia/editor/scenario/time_interval.hpp>
namespace Scenario
{
class IntervalModel;
}
namespace Execution
{
class BaseScenarioElement;
class SCORE_PLUGIN_ENGINE_EXPORT DefaultClock
{
public:
  DefaultClock(const Context& ctx);
  ~DefaultClock();

  void prepareExecution(const TimeVal& t, BaseScenarioElement& bs);

  void play(const TimeVal& t, BaseScenarioElement&);
  void pause(BaseScenarioElement&);
  void resume(BaseScenarioElement&);
  void stop(BaseScenarioElement&);

private:
  const Context& context;
};
}
