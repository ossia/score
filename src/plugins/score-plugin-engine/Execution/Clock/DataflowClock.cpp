#include "DataflowClock.hpp"

#include <Device/Protocol/DeviceInterface.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Scenario/Document/Interval/IntervalExecution.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <ossia/audio/audio_parameter.hpp>
#include <ossia/audio/audio_protocol.hpp>
#include <ossia/dataflow/graph/graph_interface.hpp>

#include <Audio/Settings/Model.hpp>
#include <Execution/Settings/ExecutorModel.hpp>

#include <flicks.h>
namespace Dataflow
{
Clock::Clock(const Execution::Context& ctx)
    : Execution::Clock{ctx}
    , m_default{ctx}
    , m_plug{context.doc.plugin<Execution::DocumentPlugin>()}
{
}

Clock::~Clock() {}

void Clock::play_impl(const TimeVal& t, Execution::BaseScenarioElement& bs)
{
  m_paused = false;

  m_plug.execGraph->print(std::cerr);
  m_cur = &bs;
  m_default.play(t);

  resume_impl(bs);
}

void Clock::pause_impl(Execution::BaseScenarioElement& bs)
{
  m_paused = true;
  m_plug.audioProto().set_tick([](auto&&...) {});
  m_default.pause();
}

void Clock::resume_impl(Execution::BaseScenarioElement& bs)
{
  m_paused = false;
  m_default.resume();
  auto tick = m_plug.settings.getTick();
  auto commit = m_plug.settings.getCommit();

  ossia::tick_setup_options opt;
  if (tick == Execution::Settings::TickPolicies{}.Buffer)
    opt.tick = ossia::tick_setup_options::Buffer;
  else if (tick == Execution::Settings::TickPolicies{}.ScoreAccurate)
    opt.tick = ossia::tick_setup_options::ScoreAccurate;
  else if (tick == Execution::Settings::TickPolicies{}.Precise)
    opt.tick = ossia::tick_setup_options::Precise;

  if (commit == Execution::Settings::CommitPolicies{}.Default)
    opt.commit = ossia::tick_setup_options::Default;
  else if (commit == Execution::Settings::CommitPolicies{}.Ordered)
    opt.commit = ossia::tick_setup_options::Ordered;
  else if (commit == Execution::Settings::CommitPolicies{}.Priorized)
    opt.commit = ossia::tick_setup_options::Priorized;
  else if (commit == Execution::Settings::CommitPolicies{}.Merged)
    opt.commit = ossia::tick_setup_options::Merged;

  if (m_plug.settings.getBench() && m_plug.bench)
  {
    auto tick = ossia::make_tick(
        opt,
        *m_plug.execState,
        *m_plug.execGraph,
        *m_cur->baseInterval().OSSIAInterval());

    m_plug.audioProto().set_tick([tick, plug = &m_plug](auto&&... args) {
      // Run some commands if they have been submitted.
      Execution::ExecutionCommand c;
      while (plug->context().executionQueue.try_dequeue(c))
      {
        c();
      }

      auto& bench = *plug->bench;
      static int i = 0;
      if (i % 50 == 0)
      {
        bench.measure = true;
        auto t0 = std::chrono::steady_clock::now();
        tick(args...);
        auto t1 = std::chrono::steady_clock::now();
        auto total
            = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0)
                  .count();

        plug->slot_bench(bench, total);
        for (auto& p : bench)
        {
          p.second = {};
        }
      }
      else
      {
        bench.measure = false;
        tick(args...);
      }

      i++;
    });
  }
  else
  {
    auto tick = ossia::make_tick(
        opt,
        *m_plug.execState,
        *m_plug.execGraph,
        *m_cur->baseInterval().OSSIAInterval());

    m_plug.audioProto().set_tick([tick, plug = &m_plug](auto&&... args) {
      // Run some commands if they have been submitted.
      Execution::ExecutionCommand c;
      while (plug->context().executionQueue.try_dequeue(c))
      {
        c();
      }

      tick(args...);
    });
  }


  m_plug.execGraph->print(std::cerr);
}

void Clock::stop_impl(Execution::BaseScenarioElement& bs)
{
  m_paused = false;

  auto& proto = m_plug.audioProto();
  proto.set_tick([](unsigned long, double) {});

  m_plug.finished();
  m_default.stop();
}

bool Clock::paused() const
{
  return m_paused;
}

std::unique_ptr<Execution::Clock>
ClockFactory::make(const Execution::Context& ctx)
{
  return std::make_unique<Clock>(ctx);
}

Execution::time_function
ClockFactory::makeTimeFunction(const score::DocumentContext& ctx) const
{
  return [=](const TimeVal& v) -> ossia::time_value {
    // Go from milliseconds to flicks
    // 1000 ms = sr samples
    // x ms    = k samples
    return v.isInfinite() ? ossia::Infinite
                          : ossia::time_value{int64_t(std::llround(v.msec() * 705600.))};
  };
}

Execution::reverse_time_function
ClockFactory::makeReverseTimeFunction(const score::DocumentContext& ctx) const
{
  return [=](const ossia::time_value& v) -> TimeVal {
    return v.infinite() ? TimeVal::infinite()
                        : TimeVal::fromMsecs(v.impl / 705600.);
  };
}

QString ClockFactory::prettyName() const
{
  return QObject::tr("Audio");
}
}
