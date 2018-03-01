#include "DataflowClock.hpp"
#include <Engine/Executor/ExecutorContext.hpp>
#include <Engine/Executor/DocumentPlugin.hpp>
#include <Engine/Executor/BaseScenarioComponent.hpp>
#include <Engine/Executor/IntervalComponent.hpp>
#include <Engine/Executor/Settings/ExecutorModel.hpp>
#include <Engine/Executor/BaseScenarioComponent.hpp>
#include <Engine/Executor/IntervalComponent.hpp>
#include <ossia/dataflow/audio_parameter.hpp>
#include <ossia/dataflow/audio_protocol.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Engine/Protocols/OSSIADevice.hpp>
#include <ossia/dataflow/graph/graph.hpp>
#include <QFile>
#include <QPointer>
#include <ossia/network/midi/midi_device.hpp>
#include <ossia/dataflow/graph/tick_methods.hpp>
#include <Engine/Executor/Settings/ExecutorModel.hpp>
namespace Dataflow
{
Clock::Clock(
    const Engine::Execution::Context& ctx):
  ClockManager{ctx},
  m_default{ctx},
  m_plug{context.doc.plugin<Engine::Execution::DocumentPlugin>()}
{
  auto& bs = context.scenario;
  if(!bs.active())
    return;
}

Clock::~Clock()
{
}


void Clock::play_impl(
    const TimeVal& t,
    Engine::Execution::BaseScenarioElement& bs)
{
  m_paused = false;

  m_plug.execGraph->print(std::cerr);
  m_cur = &bs;
  m_default.play(t);

  resume_impl(bs);
}

void Clock::pause_impl(
    Engine::Execution::BaseScenarioElement& bs)
{
  m_paused = true;
  m_plug.audioProto().ui_tick = [] (auto&&...) {};
  m_plug.audioProto().replace_tick = true;
  qDebug("pause");
  m_default.pause();
}

template<typename... Args>
smallfun::function<void(unsigned long, double), 128> make_ui_tick(const Engine::Execution::Settings::Model& settings, Args&&... args)
{
  using namespace Engine::Execution::Settings;
  auto tick = settings.getTick();
  auto commit = settings.getCommit();

  if(commit == CommitPolicies{}.Default)
  {
    static constexpr const auto commit_policy = &ossia::execution_state::commit;
    if(tick == TickPolicies{}.Buffer)
      return ossia::buffer_tick<commit_policy>{args...};
    else if(tick == TickPolicies{}.Precise)
      return ossia::precise_score_tick<commit_policy>{args...};
    else if(tick == TickPolicies{}.ScoreAccurate)
      return ossia::split_score_tick<commit_policy>{args...};
    else
      return ossia::buffer_tick<commit_policy>{args...};
  }
  else if(commit == CommitPolicies{}.Ordered)
  {
    static constexpr const auto commit_policy = &ossia::execution_state::commit_ordered;
    if(tick == TickPolicies{}.Buffer)
      return ossia::buffer_tick<commit_policy>{args...};
    else if(tick == TickPolicies{}.Precise)
      return ossia::precise_score_tick<commit_policy>{args...};
    else if(tick == TickPolicies{}.ScoreAccurate)
      return ossia::split_score_tick<commit_policy>{args...};
    else
      return ossia::buffer_tick<commit_policy>{args...};
  }
  else if(commit == CommitPolicies{}.Priorized)
  {
    static constexpr const auto commit_policy = &ossia::execution_state::commit_priorized;
    if(tick == TickPolicies{}.Buffer)
      return ossia::buffer_tick<commit_policy>{args...};
    else if(tick == TickPolicies{}.Precise)
      return ossia::precise_score_tick<commit_policy>{args...};
    else if(tick == TickPolicies{}.ScoreAccurate)
      return ossia::split_score_tick<commit_policy>{args...};
    else
      return ossia::buffer_tick<commit_policy>{args...};
  }
  else if(commit == CommitPolicies{}.Merged)
  {
    static constexpr const auto commit_policy = &ossia::execution_state::commit_merged;
    if(tick == TickPolicies{}.Buffer)
      return ossia::buffer_tick<commit_policy>{args...};
    else if(tick == TickPolicies{}.Precise)
      return ossia::precise_score_tick<commit_policy>{args...};
    else if(tick == TickPolicies{}.ScoreAccurate)
      return ossia::split_score_tick<commit_policy>{args...};
    else
      return ossia::buffer_tick<commit_policy>{args...};
  }
  return ossia::buffer_tick<&ossia::execution_state::commit>{args...};
}

void Clock::resume_impl(
    Engine::Execution::BaseScenarioElement& bs)
{
  m_paused = false;
  m_default.resume();
  auto tick = m_plug.context().settings.getTick();

  // sorry padre for I have sinned
  m_plug.audioProto().ui_tick = make_ui_tick(
                                  m_plug.context().settings,
                                  *m_plug.execState, *m_plug.execGraph, *m_cur->baseInterval().OSSIAInterval());

  m_plug.audioProto().replace_tick = true;
  qDebug("resume");
}

void Clock::stop_impl(
    Engine::Execution::BaseScenarioElement& bs)
{
  m_paused = false;
  QPointer<Engine::Execution::DocumentPlugin> plug = &m_plug;

  m_plug.audioProto().ui_tick = [=,&proto=m_plug.audioProto()] (unsigned long, double) {
    plug->finished();
    proto.ui_tick = [] (unsigned long, double) {};
    proto.replace_tick = true;

#if defined(SCORE_BENCHMARK)
#endif

  };

#if defined(SCORE_BENCHMARK)
  CALLGRIND_DUMP_STATS;
#endif
  m_plug.audioProto().replace_tick = true;
  m_default.stop();
}

bool Clock::paused() const
{
  return m_paused;
}

std::unique_ptr<Engine::Execution::ClockManager> ClockFactory::make(
    const Engine::Execution::Context& ctx)
{
  return std::make_unique<Clock>(ctx);
}

Engine::Execution::time_function
ClockFactory::makeTimeFunction(const score::DocumentContext& ctx) const
{
  auto rate = ctx.plugin<Engine::Execution::DocumentPlugin>().audioProto().rate;
  return [=] (const TimeVal& v) -> ossia::time_value {
    // Go from milliseconds to samples
    // 1000 ms = sr samples
    // x ms    = k samples
    return v.isInfinite()
        ? ossia::Infinite
        : ossia::time_value(std::llround(rate * v.msec() / 1000.));
  };
}

Engine::Execution::reverse_time_function
ClockFactory::makeReverseTimeFunction(const score::DocumentContext& ctx) const
{
  auto rate = ctx.plugin<Engine::Execution::DocumentPlugin>().audioProto().rate;
  return [=] (const ossia::time_value& v) -> TimeVal {
    return v.infinite()
        ? TimeVal{PositiveInfinity{}}
        : TimeVal::fromMsecs(1000. * v.impl / rate);
  };
}

QString ClockFactory::prettyName() const
{
  return QObject::tr("Dataflow");
}

}
