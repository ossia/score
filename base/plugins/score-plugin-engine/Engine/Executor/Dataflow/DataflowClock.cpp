#include "DataflowClock.hpp"
#include <Engine/Executor/ExecutorContext.hpp>
#include <Engine/Executor/DocumentPlugin.hpp>
#include <Engine/Executor/BaseScenarioComponent.hpp>
#include <Engine/Executor/IntervalComponent.hpp>
#include <Engine/Executor/Settings/ExecutorModel.hpp>
#include <Engine/Executor/BaseScenarioComponent.hpp>
#include <Engine/Executor/IntervalComponent.hpp>
#include <boost/graph/graphviz.hpp>
#include <ossia/dataflow/audio_parameter.hpp>
#include <ossia/dataflow/audio_protocol.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Engine/Protocols/OSSIADevice.hpp>
#include <ossia/dataflow/graph.hpp>
#include <QPointer>
#include <ossia/network/midi/midi_device.hpp>
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

  std::stringstream s;
  auto& g = m_plug.execGraph->impl();
  boost::write_graphviz(s, g, [&] (auto& out, const auto& v) {
    out << "[label=\"" << g[v]->label() << "\"]";
  },
  [] (auto&&...) {});

  std::cerr << s.str() << std::endl;
  m_cur = &bs;

  m_plug.execState.samples_since_start = 0;
  m_plug.execState.start_date = std::chrono::high_resolution_clock::now();
  m_plug.execState.cur_date = m_plug.execState.start_date;
  m_plug.execState.clear_devices();
  m_plug.execState.register_device(&m_plug.midi_dev);
  m_plug.execState.register_device(&m_plug.audio_dev);
  for(auto dev : context.devices.list().devices()) {
    if(auto od = dynamic_cast<Engine::Network::OSSIADevice*>(dev))
      if(auto d = od->getDevice())
      {
        if(auto midi_dev = dynamic_cast<ossia::net::midi::midi_device*>(d))
          m_plug.execState.register_device(midi_dev);
        else
          m_plug.execState.register_device(d);
      }
  }
  m_default.play(t);

  resume_impl(bs);
}

void Clock::pause_impl(
    Engine::Execution::BaseScenarioElement& bs)
{
  m_paused = true;
  m_plug.audioProto().ui_tick = {};
  m_plug.audioProto().replace_tick = true;
  qDebug("pause");
  m_default.pause();
}

void Clock::resume_impl(
    Engine::Execution::BaseScenarioElement& bs)
{
  m_paused = false;
  m_default.resume();
  m_plug.audioProto().ui_tick = [&st=m_plug.execState,&g=m_plug.execGraph,itv=m_cur->baseInterval().OSSIAInterval()] (unsigned long frameCount) {
    st.clear();
    st.get_new_values();
    st.samples_since_start += frameCount;
    st.cur_date = std::chrono::high_resolution_clock::now();
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

  m_plug.audioProto().ui_tick = [=,&proto=m_plug.audioProto()] (unsigned long) {
    emit plug->finished();
    proto.ui_tick = { };
    proto.replace_tick = true;
  };
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

std::function<ossia::time_value (const TimeVal&)>
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

std::function<TimeVal(const ossia::time_value&)>
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
