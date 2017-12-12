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
#include <QPointer>
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
  auto& g = m_plug.execGraph->m_graph;
  boost::write_graphviz(s, g, [&] (auto& out, const auto& v) {
    out << "[label=\"" << typeid(g[v].get()).name() << "\"]";
  },
  [] (auto&&...) {});

  std::cerr << s.str() << std::endl;
  m_cur = &bs;

  m_plug.execState.clear_devices();
  m_plug.execState.register_device(&m_plug.midi_dev);
  m_plug.execState.register_device(&m_plug.audio_dev);
  for(auto dev : context.devices.list().devices()) {
    if(auto od = dynamic_cast<Engine::Network::OSSIADevice*>(dev))
      if(auto d = od->getDevice())
        m_plug.execState.register_device(d);
  }
  m_default.play(t);
  for(auto& n : m_plug.execGraph->m_nodes.left)
  {
    for(auto& port : n.second->inputs())
    {
      if(auto vp = port->data.target<ossia::value_port>())
      {
        if(vp->is_event)
        {
          if(auto addr = port->address.target<ossia::net::parameter_base*>())
            m_plug.execState.register_parameter(**addr);
        }
      }
    }

  }

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
  QPointer<Scenario::ScenarioDocumentModel> doc = &m_plug.context().doc.model<Scenario::ScenarioDocumentModel>();
  m_plug.context().executionQueue.enqueue([=] {
    if(doc)
    {
      for(Process::Cable& cbl : doc->cables)
      {
        cbl.source_node.reset();
        cbl.sink_node.reset();
        cbl.source_port.reset();
        cbl.sink_port.reset();
        cbl.exec.reset();
      }
    }

    if (plug)
    {
      plug->inlets.clear();
      plug->outlets.clear();
      plug->m_cables.clear();
      plug->execGraph->clear();
    }

    if (doc)
    {
      for (Process::Cable& cable : doc->cables)
      {
        if(cable.source_node)
        {
          cable.source_node->clear();
          cable.source_node.reset();
        }
        if(cable.sink_node)
        {
          cable.sink_node->clear();
          cable.sink_node.reset();
        }

        if(cable.exec)
        {
          cable.exec->clear();
          cable.exec.reset();
        }
      }
    }
    if (plug)
    {
      plug->execState.clear();
      plug->execState.globalState.clear();
      plug->execState.messages.clear();
      plug->execState.mess_values.clear();
    }

    emit plug->finished();

  });
  m_plug.audioProto().ui_tick = { };
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
