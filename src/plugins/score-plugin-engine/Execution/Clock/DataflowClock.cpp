#include "DataflowClock.hpp"

#include <Device/Protocol/DeviceInterface.hpp>

#include <Process/ExecutionAction.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Scenario/Document/Interval/IntervalExecution.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <Audio/AudioApplicationPlugin.hpp>
#include <Audio/AudioTick.hpp>
#include <Audio/Settings/Model.hpp>
#include <Execution/ExecutionTick.hpp>
#include <Execution/Settings/ExecutorModel.hpp>

#include <ossia/audio/audio_parameter.hpp>
#include <ossia/audio/audio_protocol.hpp>
#include <ossia/dataflow/graph/graph_interface.hpp>
#include <ossia/detail/flicks.hpp>

#include <flicks.h>
namespace Dataflow
{
Clock::Clock(const Execution::Context& ctx)
    : Execution::Clock{ctx}
    , m_default{ctx}
    , m_audio{context.doc.app.guiApplicationPlugin<Audio::ApplicationPlugin>()}
    , m_plug{context.doc.plugin<Execution::DocumentPlugin>()}
{
}

Clock::~Clock() { }

void Clock::play_impl(const TimeVal& t)
{
  m_paused = false;

  m_default.play(t, *this->scenario);

  auto tick = m_plug.settings.getTick();
  auto commit = m_plug.settings.getCommit();

  ossia::tick_setup_options opt;
  if(tick == Execution::Settings::TickPolicies{}.Buffer)
    opt.tick = ossia::tick_setup_options::Buffer;
  else if(tick == Execution::Settings::TickPolicies{}.ScoreAccurate)
    opt.tick = ossia::tick_setup_options::ScoreAccurate;
  else if(tick == Execution::Settings::TickPolicies{}.Precise)
    opt.tick = ossia::tick_setup_options::Precise;

  if(commit == Execution::Settings::CommitPolicies{}.Default)
    opt.commit = ossia::tick_setup_options::Default;
  else if(commit == Execution::Settings::CommitPolicies{}.Ordered)
    opt.commit = ossia::tick_setup_options::Ordered;
  else if(commit == Execution::Settings::CommitPolicies{}.Priorized)
    opt.commit = ossia::tick_setup_options::Priorized;
  else if(commit == Execution::Settings::CommitPolicies{}.Merged)
    opt.commit = ossia::tick_setup_options::Merged;

  if(m_plug.settings.getBench() && m_plug.contextData()->bench)
  {
    m_play_tick = Execution::makeBenchmarkTick(opt, m_plug, this->scenario);
  }
  else
  {
    m_play_tick = Execution::makeExecutionTick(opt, m_plug, this->scenario);
  }

  m_pause_tick = Audio::makePauseTick(this->context.doc.app);

  resume_impl();
}

void Clock::pause_impl()
{
  m_paused = true;
  if(auto e = m_audio.audio.get())
    e->set_tick(ossia::audio_engine::fun_type{m_pause_tick});
  m_default.pause(*this->scenario);
}

void Clock::resume_impl()
{
  auto e = m_audio.audio.get();
  if(!e)
    return;

  m_paused = false;
  m_default.resume(*this->scenario);

  e->set_tick(ossia::audio_engine::fun_type{m_play_tick});
}

void Clock::stop_impl()
{
  m_paused = false;

  if(auto e = m_audio.audio.get())
    e->set_tick(ossia::audio_engine::fun_type{m_pause_tick});

  m_default.stop(*this->scenario);
  m_plug.finished();
}

bool Clock::paused() const
{
  return m_paused;
}

std::unique_ptr<Execution::Clock> ClockFactory::make(const Execution::Context& ctx)
{
  return std::make_unique<Clock>(ctx);
}

Execution::time_function
ClockFactory::makeTimeFunction(const score::DocumentContext& ctx) const
{
  return [=](const TimeVal& v) -> ossia::time_value {
    return v;
    /*
    // Go from milliseconds to flicks
    // 1000 ms = sr samples
    // x ms    = k samples
    return v.infinite() ? ossia::Infinite
                        : ossia::time_value{int64_t(std::llround(v.msec() *
    ossia::flicks_per_millisecond<double>))};
                        */
  };
}

Execution::reverse_time_function
ClockFactory::makeReverseTimeFunction(const score::DocumentContext& ctx) const
{
  return [=](const ossia::time_value& v) -> TimeVal {
    return v;
    /*
    static const constexpr double ratio = 1. /
    ossia::flicks_per_millisecond<double>;

    return v.infinite() ? TimeVal::infinite()
                        : TimeVal::fromMsecs(v.impl * ratio);
    */
  };
}

QString ClockFactory::prettyName() const
{
  return QObject::tr("Audio");
}
}
