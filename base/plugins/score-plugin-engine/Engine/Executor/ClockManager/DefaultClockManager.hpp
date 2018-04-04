#pragma once
#include <ossia/editor/scenario/clock.hpp>
#include <ossia/editor/scenario/time_interval.hpp>
#include <Engine/Executor/ClockManager/ClockManagerFactory.hpp>
namespace Scenario
{
class IntervalModel;
}
namespace Engine
{
namespace Execution
{
class BaseScenarioElement;
class SCORE_PLUGIN_ENGINE_EXPORT DefaultClockManager final
    : public ClockManager
{
public:
  DefaultClockManager(const Context& ctx);

  virtual ~DefaultClockManager();

  void prepareExecution(
      const TimeVal& t, BaseScenarioElement& bs);
private:
  void play_impl(const TimeVal& t, BaseScenarioElement&) override;
  void pause_impl(BaseScenarioElement&) override;
  void resume_impl(BaseScenarioElement&) override;
  void stop_impl(BaseScenarioElement&) override;

};

class ControlClock final
    : public ClockManager
{
public:
  ControlClock(const Context& ctx);

  private:
      // Clock interface
      void play_impl(
              const TimeVal& t,
              Engine::Execution::BaseScenarioElement&) override;
      void pause_impl(Engine::Execution::BaseScenarioElement&) override;
      void resume_impl(Engine::Execution::BaseScenarioElement&) override;
      void stop_impl(Engine::Execution::BaseScenarioElement&) override;
      bool paused() const override;

      Engine::Execution::DefaultClockManager m_default;
      ossia::clock m_clock;
};

class SCORE_PLUGIN_ENGINE_EXPORT ControlClockFactory final
    : public ClockManagerFactory
{
  SCORE_CONCRETE("583e9c52-e136-46b6-852f-7eef2993e9eb")

public:
  virtual ~ControlClockFactory();
  QString prettyName() const override;
  std::unique_ptr<ClockManager>
  make(const Engine::Execution::Context& ctx) override;

  time_function
  makeTimeFunction(const score::DocumentContext& ctx) const override;
  reverse_time_function
  makeReverseTimeFunction(const score::DocumentContext& ctx) const override;
};
}
}
