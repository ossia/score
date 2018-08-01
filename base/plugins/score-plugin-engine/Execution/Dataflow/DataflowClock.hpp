#pragma once
#include <Execution/ClockManager/ClockManagerFactory.hpp>
#include <Execution/ClockManager/DefaultClockManager.hpp>
#include <Execution/DocumentPlugin.hpp>
namespace Process
{
class Cable;
}
namespace Dataflow
{
class DocumentPlugin;
class Clock final
    : public Execution::ClockManager
    , public Nano::Observer
{
public:
  Clock(const Execution::Context& ctx);

  ~Clock() override;

private:
  // Clock interface
  void play_impl(
      const TimeVal& t, Execution::BaseScenarioElement&) override;
  void pause_impl(Execution::BaseScenarioElement&) override;
  void resume_impl(Execution::BaseScenarioElement&) override;
  void stop_impl(Execution::BaseScenarioElement&) override;
  bool paused() const override;

  Execution::DefaultClockManager m_default;
  Execution::DocumentPlugin& m_plug;
  Execution::BaseScenarioElement* m_cur{};
  bool m_paused{};
};

class ClockFactory final : public Execution::ClockManagerFactory
{
  SCORE_CONCRETE("e9ae6dec-a10f-414f-9060-b21d15b5d58d")

  QString prettyName() const override;
  std::unique_ptr<Execution::ClockManager>
  make(const Execution::Context& ctx) override;

  Execution::time_function
  makeTimeFunction(const score::DocumentContext& ctx) const override;
  Execution::reverse_time_function
  makeReverseTimeFunction(const score::DocumentContext& ctx) const override;
};
}
