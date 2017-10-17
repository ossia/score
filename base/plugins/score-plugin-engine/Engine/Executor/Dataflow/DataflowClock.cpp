#include "DataflowClock.hpp"
#include <Engine/Executor/ExecutorContext.hpp>
#include <Engine/Executor/DocumentPlugin.hpp>
#include <Engine/Executor/BaseScenarioComponent.hpp>
#include <Engine/Executor/IntervalComponent.hpp>
#include <Engine/Executor/Settings/ExecutorModel.hpp>
#include <Engine/Executor/BaseScenarioComponent.hpp>
#include <Engine/Executor/IntervalComponent.hpp>
#include <Dataflow/UI/ConstraintNode.hpp>
#include <Dataflow/DocumentPlugin.hpp>
#include <boost/graph/graphviz.hpp>
#include <portaudio.h>
#include <ossia/dataflow/audio_parameter.hpp>
#include <ossia/dataflow/audio_protocol.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
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
  boost::write_graphviz(s, m_plug.execGraph->m_graph, [&] (auto& out, const auto& v) {
    out << "[label=\"" << (void*)m_plug.execGraph->m_graph[v].get() << "\"]";
  },
  [] (auto&&...) {});

  std::cerr << s.str() << std::endl;
  m_cur = &bs;

  m_plug.execState.globalState.clear();
  m_plug.execState.globalState.push_back(&m_plug.midi_dev);
  m_plug.execState.globalState.push_back(&m_plug.audio_dev);
  m_default.play(t);

  m_plug.audioProto().ui_tick = [this] (unsigned long frameCount) {
    m_plug.execState.clear();
    m_cur->baseInterval().OSSIAInterval()->tick(ossia::time_value(frameCount));
    m_plug.execGraph->state(m_plug.execState);
    m_plug.execState.commit();
  };

  m_plug.audioProto().replace_tick = true;
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
  m_plug.audioProto().ui_tick = [this] (unsigned long frameCount) {
    m_plug.execState.clear();
    m_cur->baseInterval().OSSIAInterval()->tick(ossia::time_value(frameCount));
    m_plug.execGraph->state(m_plug.execState);
    m_plug.execState.commit();
  };

  m_plug.audioProto().replace_tick = true;
  qDebug("resume");
}

void Clock::stop_impl(
    Engine::Execution::BaseScenarioElement& bs)
{
  m_paused = false;
  m_plug.context().executionQueue.enqueue([&] {
    auto& model = m_plug.context().doc.model<Scenario::ScenarioDocumentModel>();
    for(Process::Cable& cbl : model.cables)
    {
      cbl.source_node.reset();
      cbl.sink_node.reset();
      cbl.source_port.reset();
      cbl.sink_port.reset();
      cbl.exec.reset();
    }
    m_plug.inlets.clear();
    m_plug.outlets.clear();
    m_plug.m_cables.clear();
    m_plug.execGraph->clear();
    m_plug.execGraph = std::make_shared<ossia::graph>();

    for(auto& cable : model.cables)
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
