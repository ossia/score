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
namespace Dataflow
{
class DocumentPlugin;
class Clock final
    : public Execution::Clock
    , public Nano::Observer
{
public:
  Clock(const Execution::Context& ctx);

  ~Clock() override;

private:
  // Clock interface
  void play_impl(const TimeVal& t) override;
  void pause_impl() override;
  void resume_impl() override;
  void stop_impl() override;
  bool paused() const override;

  Execution::DefaultClock m_default;
  Audio::ApplicationPlugin& m_audio;
  Execution::DocumentPlugin& m_plug;
  bool m_paused{};

  ossia::audio_engine::fun_type m_play_tick{};
  ossia::audio_engine::fun_type m_pause_tick{};
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
