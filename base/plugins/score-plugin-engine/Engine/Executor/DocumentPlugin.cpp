// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include "BaseScenarioComponent.hpp"
#include "DocumentPlugin.hpp"
#include <Engine/Executor/IntervalComponent.hpp>
#include <Engine/Executor/StateProcessComponent.hpp>
#include <Engine/Executor/Settings/ExecutorModel.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <score/plugins/documentdelegate/plugin/DocumentPlugin.hpp>

#include <Engine/ApplicationPlugin.hpp>
#include <ossia/dataflow/audio_protocol.hpp>
#include <ossia/editor/scenario/time_interval.hpp>
namespace Engine
{
namespace Execution
{
DocumentPlugin::DocumentPlugin(
    const score::DocumentContext& ctx,
    Id<score::DocumentPlugin> id,
    QObject* parent)
    : score::DocumentPlugin{ctx, std::move(id),
                             "OSSIADocumentPlugin", parent}
    , audioproto{new ossia::audio_protocol}
    , audio_dev{std::unique_ptr<ossia::net::protocol_base>(audioproto), "audio"}
    , midi_dev{std::make_unique<ossia::net::multiplex_protocol>(), "midi"}
    , m_ctx{
          ctx, m_base,
          ctx.plugin<Explorer::DeviceDocumentPlugin>(),
          ctx.app.interfaces<ProcessComponentFactoryList>(),
          ctx.app.interfaces<StateProcessComponentFactoryList>(),
          {}, {},
          m_editionQueue, *this
      }
    , m_base{m_ctx, this}
{
  midi_ins.push_back(ossia::net::create_parameter<ossia::midi_generic_parameter>(midi_dev.get_root_node(), "/0/in"));
  midi_outs.push_back(ossia::net::create_parameter<ossia::midi_generic_parameter>(midi_dev.get_root_node(), "/0/out"));

  execGraph = std::make_shared<ossia::graph>();
  audioproto->reload();

  auto& model = ctx.model<Scenario::ScenarioDocumentModel>();
  model.cables.mutable_added.connect<DocumentPlugin, &DocumentPlugin::on_cableCreated>(*this);
  model.cables.removing.connect<DocumentPlugin, &DocumentPlugin::on_cableRemoved>(*this);
}

void DocumentPlugin::on_cableCreated(Process::Cable& c)
{
  connectCable(c);
}

void DocumentPlugin::on_cableRemoved(const Process::Cable& c)
{
  auto cable = c.exec;
  auto graph = execGraph;

  context().executionQueue.enqueue([cable,graph] {
    graph->disconnect(cable);
  });
}

void DocumentPlugin::connectCable(Process::Cable& cable)
{
    std::cerr << "\n\nConnect 2\n";

    if(cable.source())
    {
      auto it = nodes.find(cable.source());
      if(it != nodes.end())
        cable.source_node = it->second;
    }
    if(cable.sink())
    {
      auto it = nodes.find(cable.sink());
      if(it != nodes.end())
        cable.sink_node = it->second;
    }

    std::cerr << cable.source_node.get() << " && " << cable.sink_node.get() << "\n";
    if(cable.source_node && cable.sink_node)
    {
      std::cerr << "\n\nConnect 3\n";

      context().executionQueue.enqueue(
            [type=cable.type()
            ,src=cable.source_node
            ,snk=cable.sink_node
            ,inlt=cable.sink()->num
            ,outlt=cable.source()->num
            ,graph=execGraph
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


DocumentPlugin::~DocumentPlugin()
{
  if (m_base.active())
  {
    m_base.baseInterval().stop();
    clear();
  }
  audioproto->stop();
}

void DocumentPlugin::reload(Scenario::IntervalModel& cst)
{
  if (m_base.active())
  {
    m_base.baseInterval().stop();
  }
  clear();
  auto& ctx = score::DocumentPlugin::context();
  auto& settings = ctx.app.settings<Engine::Execution::Settings::Model>();
  m_ctx.time = settings.makeTimeFunction(ctx);
  m_ctx.reverseTime = settings.makeReverseTimeFunction(ctx);
  auto parent = dynamic_cast<Scenario::ScenarioInterface*>(cst.parent());
  SCORE_ASSERT(parent);
  m_base.init(BaseScenarioRefContainer{cst, *parent});

  ossia::graph_node& n = *m_base.baseInterval().OSSIAInterval()->node;
  n.outputs()[0]->address = ossia::net::find_node(this->audio_dev, "/out/main")->get_parameter();

  auto& model = context().doc.model<Scenario::ScenarioDocumentModel>();
  //qDebug() << context().document.findChildren<Process::Node*>();
  for(auto& cable : model.cables)
  {
    connectCable(cable);
  }

  runAllCommands();
}

void DocumentPlugin::clear()
{
  if(m_base.active())
  {
    auto& model = context().doc.model<Scenario::ScenarioDocumentModel>();
    for(Process::Cable& cbl : model.cables)
    {
      cbl.source_node.reset();
      cbl.sink_node.reset();
      cbl.exec.reset();
    }
    m_cables.clear();
    execGraph->clear();
    execGraph = std::make_shared<ossia::graph>();

    runAllCommands();
    m_base.cleanup();
    runAllCommands();

  }
}

void DocumentPlugin::on_documentClosing()
{
  if (m_base.active())
  {
    m_base.baseInterval().stop();
    m_ctx.context().doc.app.guiApplicationPlugin<Engine::ApplicationPlugin>().on_stop();
    clear();
  }
}

const BaseScenarioElement& DocumentPlugin::baseScenario() const
{
  return m_base;
}

bool DocumentPlugin::isPlaying() const
{
  return m_base.active();
}

void DocumentPlugin::runAllCommands() const
{
  bool ok = false;
  ExecutionCommand com;
  do {
    ok = m_editionQueue.try_dequeue(com);
    if(ok && com)
      com();
  } while(ok);
}
}
}
