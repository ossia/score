#include "DataflowClock.hpp"
#include <Engine/Executor/ExecutorContext.hpp>
#include <Engine/Executor/DocumentPlugin.hpp>
#include <Engine/Executor/BaseScenarioComponent.hpp>
#include <Engine/Executor/ConstraintComponent.hpp>
#include <Engine/Executor/Settings/ExecutorModel.hpp>
#include <Engine/Executor/BaseScenarioComponent.hpp>
#include <Engine/Executor/ConstraintComponent.hpp>
#include <Dataflow/UI/ConstraintNode.hpp>
#include <Dataflow/DocumentPlugin.hpp>
#include <boost/graph/graphviz.hpp>
#include <portaudio.h>
#include <ossia/dataflow/audio_parameter.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
namespace Dataflow
{
Clock::Clock(
    const Engine::Execution::Context& ctx):
  ClockManager{ctx},
  m_default{ctx},
  m_plug{context.doc.plugin<Dataflow::DocumentPlugin>()}
{
  auto& bs = context.scenario;
  if(!bs.active())
    return;

  auto& model = m_plug.context().model<Scenario::ScenarioDocumentModel>();
  model.cables.mutable_added.connect<Clock, &Clock::on_cableCreated>(*this);
  model.cables.removing.connect<Clock, &Clock::on_cableRemoved>(*this);
}

Clock::~Clock()
{
  auto& model = m_plug.context().model<Scenario::ScenarioDocumentModel>();
  for(Process::Cable& cbl : model.cables)
  {
    cbl.source_node.reset();
    cbl.sink_node.reset();
    cbl.exec.reset();
  }
  m_plug.execGraph->clear();
  m_plug.execGraph = std::make_shared<ossia::graph>();
}

void Clock::on_cableCreated(Process::Cable& c)
{
  connectCable(c);
}

void Clock::on_cableRemoved(const Process::Cable& c)
{
  auto cable = c.exec;
  auto graph = m_plug.execGraph;

  context.executionQueue.enqueue([cable,graph] {
    graph->disconnect(cable);
  });
}

void Clock::connectCable(Process::Cable& cable)
{
    std::cerr << "\n\nConnect 2\n";

    if(cable.source())
      cable.source_node = cable.source()->exec;
    if(cable.sink())
      cable.sink_node = cable.sink()->exec;

    std::cerr << cable.source_node.get() << " && " << cable.sink_node.get() << "\n";
    if(cable.source_node && cable.sink_node && cable.inlet() && cable.outlet())
    {
      std::cerr << "\n\nConnect 3\n";

      context.executionQueue.enqueue(
            [type=cable.type()
            ,src=cable.source_node
            ,snk=cable.sink_node
            ,inlt=*cable.inlet()
            ,outlt=*cable.outlet()
            ,graph=m_plug.execGraph
            ]
      {
        std::cerr << "\n\nConnect 4\n";
        ossia::edge_ptr edge;
        auto& outlet = src->outputs()[outlt];
        auto& inlet = snk->inputs()[inlt];
        switch(type)
        {
          case Process::CableType::ImmediateStrict:
          {
            std::cerr << "\n\nConnect ImmediateStrict\n";
            edge = ossia::make_edge(
                           ossia::immediate_strict_connection{},
                           outlet, inlet, src, snk);
            break;
          }
          case Process::CableType::ImmediateGlutton:
          {
            std::cerr << "\n\nConnect ImmediateGlutton\n";
            edge = ossia::make_edge(
                           ossia::immediate_glutton_connection{},
                           outlet, inlet, src, snk);
            break;
          }
          case Process::CableType::DelayedStrict:
          {
            std::cerr << "\n\nConnect DelayedStrict\n";
            edge = ossia::make_edge(
                           ossia::delayed_strict_connection{},
                           outlet, inlet, src, snk);
            break;
          }
          case Process::CableType::DelayedGlutton:
          {
            std::cerr << "\n\nConnect DelayedGlutton\n";
            edge = ossia::make_edge(
                           ossia::delayed_glutton_connection{},
                           outlet, inlet, src, snk);
            break;
          }
        }

        graph->connect(edge);
    });
    }
}

void Clock::play_impl(
    const TimeVal& t,
    Engine::Execution::BaseScenarioElement& bs)
{
  m_paused = false;

  auto& model = m_plug.context().model<Scenario::ScenarioDocumentModel>();
  qDebug() << m_plug.context().document.findChildren<Process::Node*>();
  for(auto& cable : model.cables)
  {
    connectCable(cable);
  }

  std::stringstream s;
  boost::write_graphviz(s, m_plug.execGraph->m_graph, [&] (auto& out, const auto& v) {
    out << "[label=\"" << (void*)m_plug.execGraph->m_graph[v].get() << "\"]";
  },
  [] (auto&&...) {});

  std::cerr << s.str() << std::endl;
  m_cur = &bs;

  m_default.play(t);

  m_plug.audioProto().ui_tick = [this] (unsigned long frameCount) {
    m_plug.execState.clear();
    m_cur->baseConstraint().OSSIAConstraint()->tick(ossia::time_value(frameCount));
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
    m_cur->baseConstraint().OSSIAConstraint()->tick(ossia::time_value(frameCount));
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
  auto plug_ptr = &m_plug;
  m_plug.audioProto().ui_tick = [plug_ptr] (unsigned long)
  {
    DocumentPlugin& plug = *plug_ptr;
    plug.execGraph->clear();
    plug.execGraph = std::make_shared<ossia::graph>();

    auto& model = plug.context().model<Scenario::ScenarioDocumentModel>();
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

    for(auto cld : plug.context().document.findChildren<Process::Node*>())
    {
      if(cld->exec)
      {
        cld->exec->clear();
        cld->exec.reset();
      }
    }

    plug.audioProto().ui_tick = { };
    plug.audioProto().replace_tick = true;
    qDebug("everything is clear");
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
ClockFactory::makeTimeFunction(const iscore::DocumentContext& ctx) const
{
  auto rate = ctx.plugin<Dataflow::DocumentPlugin>().audioProto().rate;
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
ClockFactory::makeReverseTimeFunction(const iscore::DocumentContext& ctx) const
{
  auto rate = ctx.plugin<Dataflow::DocumentPlugin>().audioProto().rate;
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
