#pragma once
#include <ossia/editor/scenario/clock.hpp>
#include <ossia/editor/scenario/time_interval.hpp>

#include <Execution/Clock/ClockFactory.hpp>
namespace Scenario
{
class IntervalModel;
}
namespace Execution
{
class BaseScenarioElement;
class SCORE_PLUGIN_ENGINE_EXPORT DefaultClock final : public Clock
{
public:
  DefaultClock(const Context& ctx);

  virtual ~DefaultClock();

  void prepareExecution(const TimeVal& t, BaseScenarioElement& bs);

private:
  void play_impl(const TimeVal& t, BaseScenarioElement&) override;
  void pause_impl(BaseScenarioElement&) override;
  void resume_impl(BaseScenarioElement&) override;
  void stop_impl(BaseScenarioElement&) override;
};
class ControlClock final : public Clock
{
public:
  ControlClock(const Context& ctx);

private:
  // Clock interface
  void play_impl(const TimeVal& t, Execution::BaseScenarioElement&) override;
  void pause_impl(Execution::BaseScenarioElement&) override;
  void resume_impl(Execution::BaseScenarioElement&) override;
  void stop_impl(Execution::BaseScenarioElement&) override;
  bool paused() const override;

  Execution::DefaultClock m_default;
  ossia::clock m_clock;
};

class SCORE_PLUGIN_ENGINE_EXPORT ControlClockFactory final : public ClockFactory
{
  SCORE_CONCRETE("583e9c52-e136-46b6-852f-7eef2993e9eb")

public:
  virtual ~ControlClockFactory();
  QString prettyName() const override;
  std::unique_ptr<Clock> make(const Execution::Context& ctx) override;

  time_function makeTimeFunction(const score::DocumentContext& ctx) const override;
  reverse_time_function makeReverseTimeFunction(const score::DocumentContext& ctx) const override;
};
}
