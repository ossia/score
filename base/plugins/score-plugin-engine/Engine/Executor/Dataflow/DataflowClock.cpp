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

#if defined(SCORE_BENCHMARK)
#include <valgrind/callgrind.h>
#endif
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

#if defined(SCORE_BENCHMARK)
std::vector<double> m_tickDurations;

struct cycle_count_bench
{
    uint64_t rdtsc()
    {
        unsigned int lo = 0;
        unsigned int hi = 0;
        __asm__ __volatile__ (
            "lfence\n"
            "rdtsc\n"
            "lfence" : "=a"(lo), "=d"(hi)
        );
        return ((uint64_t)hi << 32) | lo;
    }

    uint64_t t0;

    cycle_count_bench()
      : t0{rdtsc()}
    {
    }

    ~cycle_count_bench()
    {
      auto t1 = rdtsc();
      m_tickDurations.push_back(t1 - t0);
    }
};

struct clock_count_bench
{
    std::chrono::time_point<std::chrono::steady_clock> t0;

    clock_count_bench()
      : t0{std::chrono::steady_clock::now()}
    {
    }

    ~clock_count_bench()
    {
      auto t1 = std::chrono::steady_clock::now();
      m_tickDurations.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
    }
};
struct callgrind_bench
{
    callgrind_bench()
    {
      CALLGRIND_START_INSTRUMENTATION;
    }
    ~callgrind_bench()
    {
      CALLGRIND_STOP_INSTRUMENTATION;
    }
};
#endif

void Clock::resume_impl(
    Engine::Execution::BaseScenarioElement& bs)
{
#if defined(SCORE_BENCHMARK)
  m_tickDurations.clear();
  m_tickDurations.reserve(100000);
#endif
  m_paused = false;
  m_default.resume();
  m_plug.audioProto().ui_tick = [&st=*m_plug.execState,&g=m_plug.execGraph,itv=m_cur->baseInterval().OSSIAInterval()] (unsigned long frameCount, double seconds) {

    st.clear_local_state();
    st.get_new_values();
    st.samples_since_start += frameCount;
    st.bufferSize = (int)frameCount;
    // we could run a syscall and call now() but that's a bit more costly.
    st.cur_date = seconds * 1e9;
    itv->tick(ossia::time_value(frameCount));
    g->state(st);
    st.commit();
  };

  m_plug.audioProto().replace_tick = true;
  qDebug("resume");
}

void Clock::stop_impl(
    Engine::Execution::BaseScenarioElement& bs)
{
  m_paused = false;
  QPointer<Engine::Execution::DocumentPlugin> plug = &m_plug;

  m_plug.audioProto().ui_tick = [=,&proto=m_plug.audioProto()] (unsigned long, double) {
    emit plug->finished();
    proto.ui_tick = [] (unsigned long, double) {};
    proto.replace_tick = true;

#if defined(SCORE_BENCHMARK)
    QFile f("/tmp/out.data");
    QTextStream s(&f);
    f.open(QIODevice::WriteOnly);
    for(auto t : m_tickDurations)
      s << t << "\n";
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
