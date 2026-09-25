#pragma once
#include <Audio/AudioTick.hpp>
#include <Execution/Clock/ClockFactory.hpp>
#include <Execution/Clock/DefaultClock.hpp>
#include <Execution/DocumentPlugin.hpp>
namespace Audio
{
class ApplicationPlugin;
}
namespace Process
{
class Cable;
}
namespace Execution::ManualClock
{
struct StepGate;
}
namespace Dataflow
{
class DocumentPlugin;
class SCORE_PLUGIN_ENGINE_EXPORT Clock
    : public Execution::Clock
    , public Nano::Observer
{
public:
  Clock(const Execution::Context& ctx, bool stepping = false);

  ~Clock() override;

  bool paused() const override;
  bool setStepping(bool stepping) override;
  bool stepping() const noexcept override;
  bool stepTo(double seconds) override;
  double steppedSeconds() const noexcept;

protected:
  // Clock interface
  void play_impl(const TimeVal& t) override;
  void pause_impl() override;
  void resume_impl() override;
  void stop_impl() override;

private:
  ossia::audio_engine::fun_type runningTick() const;

  Execution::DefaultClock m_default;
  Audio::ApplicationPlugin& m_audio;
  Execution::DocumentPlugin& m_plug;
  bool m_paused{};
  bool m_stepping{};

  ossia::audio_engine::fun_type m_play_tick{};
  ossia::audio_engine::fun_type m_pause_tick{};
  std::shared_ptr<Execution::ManualClock::StepGate> m_gate;
};

class ClockFactory final : public Execution::ClockFactory
{
  SCORE_CONCRETE("e9ae6dec-a10f-414f-9060-b21d15b5d58d")

public:
  QString prettyName() const override;
  std::unique_ptr<Execution::Clock> make(const Execution::Context& ctx) override;

  Execution::time_function
  makeTimeFunction(const score::DocumentContext& ctx) const override;
  Execution::reverse_time_function
  makeReverseTimeFunction(const score::DocumentContext& ctx) const override;
};
}
