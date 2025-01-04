// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "DocumentPlugin.hpp"

#include "BaseScenarioComponent.hpp"

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Scenario/Application/ScenarioActions.hpp>
#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <Scenario/Document/Interval/IntervalExecution.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/State/StateExecution.hpp>
#include <Scenario/Execution/score2OSSIA.hpp>

#include <Audio/AudioApplicationPlugin.hpp>
#include <Audio/AudioDevice.hpp>
#include <Audio/Settings/Model.hpp>
#include <Engine/ApplicationPlugin.hpp>
#include <Execution/Settings/ExecutorModel.hpp>

#include <score/actions/ActionManager.hpp>
#include <score/model/ComponentUtils.hpp>
#include <score/plugins/documentdelegate/plugin/DocumentPlugin.hpp>
#include <score/tools/Bind.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <ossia/audio/audio_protocol.hpp>
#include <ossia/dataflow/bench_map.hpp>
#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/graph/graph_interface.hpp>
#include <ossia/dataflow/graph_edge.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/detail/flicks.hpp>
#include <ossia/detail/logger.hpp>
#include <ossia/editor/scenario/time_interval.hpp>
#include <ossia/network/common/path.hpp>

#include <QCoreApplication>

#include <wobjectimpl.h>
W_REGISTER_ARGTYPE(ossia::bench_map)
W_OBJECT_IMPL(Execution::DocumentPlugin)
namespace Execution
{
DocumentPlugin::ContextData::ContextData(const score::DocumentContext& ctx)
    : setupContext{context}
    , context
{
  {}, ctx, m_created, {}, {}, m_execQueue, m_editionQueue, m_gcQueue, setupContext,
      execGraph, execState, currentTransaction
#if(__cplusplus > 201703L) && !defined(_MSC_VER)
      ,
  {
    ossia::disable_init
  }
#endif
}
{
}
DocumentPlugin::DocumentPlugin(const score::DocumentContext& ctx, QObject* parent)
    : score::DocumentPlugin{ctx, "OSSIADocumentPlugin", parent}
    , settings{ctx.app.settings<Execution::Settings::Model>()}
    , m_ctxData{std::make_shared<ContextData>(ctx)}
{
  m_ctxData->context.alias = m_ctxData;
  makeGraph();
  auto& devs = ctx.plugin<Explorer::DeviceDocumentPlugin>();
  local_device = devs.list().localDevice();
  if(auto dev = devs.list().audioDevice())
  {
    audio_device = static_cast<Dataflow::AudioDevice*>(dev);
  }
  else
  {
    audio_device = new Dataflow::AudioDevice(
        {Dataflow::AudioProtocolFactory::static_concreteKey(), "audio", {}});
    ctx.plugin<Explorer::DeviceDocumentPlugin>().list().setAudioDevice(audio_device);
  }

  devs.list().apply([this](auto& d) { on_deviceAdded(&d); });
  con(devs.list(), &Device::DeviceList::deviceAdded, this,
      &DocumentPlugin::on_deviceAdded);
  con(devs.list(), &Device::DeviceList::deviceRemoved, this, [this](auto* dev) {
    if(auto d = dev->getDevice())
      unregisterDevice(d);
  });

  connect(
      this, &DocumentPlugin::finished, this, &DocumentPlugin::on_finished,
      Qt::DirectConnection);

  auto& cstack = ctx.document.commandStack();

  connect(
      &cstack, &score::CommandStack::beginTransaction, this,
      [this] {
    qDebug("Begin transaction");

    if(m_ctxData)
    {
      SCORE_ASSERT(!m_ctxData->currentTransaction);
      m_ctxData->currentTransaction
          = std::make_shared<Execution::Transaction>(m_ctxData->context);
    }
  },
      Qt::DirectConnection);

  connect(
      &cstack, &score::CommandStack::endTransaction, this,
      [this] {
    qDebug("Submit transaction");
    if(m_ctxData)
    {
      m_ctxData->currentTransaction->run_all();
      m_ctxData->currentTransaction.reset();
    }
  },
      Qt::DirectConnection);
}

void DocumentPlugin::recreateBase()
{
  m_base = std::make_shared<BaseScenarioElement>(m_ctxData->context, this);
  connect(
      m_base.get(), &Execution::BaseScenarioElement::finished, this,
      [this] {
    auto& stop_action = context().doc.app.actions.action<Actions::Stop>();
    stop_action.action()->trigger();
      },
      Qt::QueuedConnection);
}

DocumentPlugin::~DocumentPlugin()
{
  if(m_base)
  {
    if(m_base->active())
    {
      m_base->baseInterval().stop();
      clear();
    }
  }

  if(auto devs = context().doc.findPlugin<Explorer::DeviceDocumentPlugin>())
  {
    devs->list().setAudioDevice(nullptr);
    devs->updateProxy.removeDevice(audio_device->settings());
  }
  if(audio_device)
    delete audio_device;
  if(m_ctxData)
  {
    m_ctxData->context.alias.reset();
  }
}

void DocumentPlugin::on_finished()
{
  if(m_tid != -1)
  {
    killTimer(m_tid);
    m_tid = -1;

    {
      ExecutionCommand cmd;
      while(m_ctxData->m_editionQueue.try_dequeue(cmd))
        cmd();
      GCCommand gc;
      while(m_ctxData->m_gcQueue.try_dequeue(gc))
        ;
    }
  }

  clear();

  initExecState();

  for(auto& v : m_ctxData->setupContext.runtime_connections)
  {
    v.second.clear();
  }
  m_ctxData->setupContext.runtime_connections.clear();
}

void DocumentPlugin::initExecState()
{
  m_ctxData->execState = std::make_shared<ossia::execution_state>();
  auto& devlist = score::DocumentPlugin::context()
                      .plugin<Explorer::DeviceDocumentPlugin>()
                      .list()
                      .devices();
  if(audio_device)
    m_ctxData->execState->register_device(audio_device->getDevice());
  if(local_device)
    m_ctxData->execState->register_device(local_device->getDevice());
  for(auto dev : devlist)
  {
    registerDevice(dev->getDevice());
  }
  m_ctxData->execState->apply_device_changes();
}

void DocumentPlugin::timerEvent(QTimerEvent* event)
{
  ExecutionCommand cmd;
  while(m_ctxData->m_editionQueue.try_dequeue(cmd))
    cmd();
  GCCommand gc;
  while(m_ctxData->m_gcQueue.try_dequeue(gc))
    ;
}

void DocumentPlugin::registerDevice(ossia::net::device_base* d)
{
  if(m_ctxData->execState)
    m_ctxData->execState->register_device(d);
}

void DocumentPlugin::unregisterDevice(ossia::net::device_base* d)
{
  if(m_ctxData->execState)
    m_ctxData->execState->unregister_device(d);
}

void DocumentPlugin::makeGraph()
{
  using namespace ossia;
  auto& audiosettings = this->m_context.app.settings<Audio::Settings::Model>();

  static const Execution::Settings::SchedulingPolicies sched_t;
  static const Execution::Settings::OrderingPolicies order_t;
  static const Execution::Settings::MergingPolicies merge_t;

  // note: cas qui n'ont pas de sens: dynamic avec les cas ou on append les
  // valeurs. parallel avec dynamic il manque le cas "default score order" il
  // manque le log pour dynamic

  auto sched = settings.getScheduling();

  auto& execGraph = m_ctxData->execGraph;
  auto& execState = m_ctxData->execState;
  auto& bench = m_ctxData->bench;

  if(execGraph)
    execGraph->clear();
  execGraph.reset();

  execState.reset();

  initExecState();

  execState->bufferSize = audiosettings.getBufferSize();
  execState->sampleRate = audiosettings.getRate();
  execState->modelToSamplesRatio
      = audiosettings.getRate() / ossia::flicks_per_second<double>;
  execState->samplesToModelRatio
      = ossia::flicks_per_second<double> / audiosettings.getRate();
  execState->samples_since_start = 0;
  execState->start_date = 0; // TODO set it in the first callback
  execState->cur_date = execState->start_date;

  auto& p = ossia::audio_buffer_pool::instance();
  for(int i = 0; i < 500; i++)
  {
    auto v = p.acquire();
    v.reserve(execState->bufferSize);
    p.release(std::move(v));
  }

  ossia::graph_setup_options opt;
  opt.parallel = settings.getParallel();
  opt.parallel_threads = settings.getThreads();
  if(settings.getLogging())
    opt.log = ossia::logger_ptr();
  if(settings.getBench())
  {
    bench = std::make_shared<bench_map>();
    opt.bench = bench;
    opt.bench->clear();
  }

  if(sched == sched_t.StaticFixed)
    opt.scheduling = ossia::graph_setup_options::StaticFixed;
  else if(sched == sched_t.StaticBFS)
    opt.scheduling = ossia::graph_setup_options::StaticBFS;
  else if(sched == sched_t.StaticTC)
    opt.scheduling = ossia::graph_setup_options::StaticTC;
  else if(sched == sched_t.Dynamic)
    opt.scheduling = ossia::graph_setup_options::Dynamic;

  opt.scheduling = ossia::graph_setup_options::StaticFixed;
  execGraph = ossia::make_graph(opt);
}

void DocumentPlugin::reload(bool forcePlay, Scenario::IntervalModel& cst)
{
  if(m_base)
  {
    if(m_base->active())
    {
      m_base->baseInterval().stop();
    }
  }
  clear();

  const score::DocumentContext& ctx = m_context;
  auto& settings = ctx.app.settings<Execution::Settings::Model>();

  SCORE_ASSERT(m_ctxData);
  m_ctxData->context.time = settings.makeTimeFunction(ctx);
  m_ctxData->context.reverseTime = settings.makeReverseTimeFunction(ctx);

  // Notify devices that they have to start running stuff, polling frames, etc.
  auto& devs = m_context.plugin<Explorer::DeviceDocumentPlugin>();
  devs.list().apply([](const Device::DeviceInterface& d) {
    if(auto dev = d.getDevice())
      dev->get_protocol().start_execution();
  });

  makeGraph();

  auto parent = dynamic_cast<Scenario::ScenarioInterface*>(cst.parent());
  SCORE_ASSERT(parent);

  recreateBase();
  m_base->init(forcePlay, BaseScenarioRefContainer{cst, *parent});
  m_ctxData->m_created = true;

  auto& model = context().doc.model<Scenario::ScenarioDocumentModel>();
  for(auto& cable : model.cables)
  {
    m_ctxData->setupContext.connectCable(cable);
  }

  for(auto ctl : model.statesWithControls)
  {
    auto state_comp
        = score::findComponent<Execution::StateComponentBase>(ctl->components());
    if(state_comp)
    {
      state_comp->updateControls();
    }
  }

  m_tid = startTimer(32);
  // runAllCommands();
}

void DocumentPlugin::clear()
{
  if(m_ctxData)
  {
    m_ctxData->setupContext.inlets.clear();
    m_ctxData->setupContext.outlets.clear();
    m_ctxData->setupContext.m_cables.clear();
    m_ctxData->setupContext.proc_map.clear();
  }
  // TODO do this in some shared object instead.
  m_base.reset();

  if(m_ctxData)
  {
    m_ctxData->m_created = false;
    m_ctxData->context.alias.reset();
  }
  m_ctxData.reset();
  m_ctxData = std::make_shared<ContextData>(this->m_context);
  m_ctxData->context.alias = m_ctxData;

  auto& model = this->m_context.model<Scenario::ScenarioDocumentModel>();
  model.cables.mutable_added.connect<&SetupContext::on_cableCreated>(
      m_ctxData->setupContext);
  model.cables.removing.connect<&SetupContext::on_cableRemoved>(m_ctxData->setupContext);

  // Notify devices that they have to stop running stuff, polling frames, etc.
  auto& devs = m_context.plugin<Explorer::DeviceDocumentPlugin>();
  devs.list().apply([](const Device::DeviceInterface& d) {
    if(auto dev = d.getDevice())
      dev->get_protocol().stop_execution();
  });
}

void DocumentPlugin::on_documentClosing()
{
  if(m_base && m_base->active())
  {
    m_base->baseInterval().stop();
    m_context.app.guiApplicationPlugin<Engine::ApplicationPlugin>()
        .execution()
        .request_stop();
    clear();
  }
  m_ctxData->execState.reset();
}

const std::shared_ptr<BaseScenarioElement>& DocumentPlugin::baseScenario() const noexcept
{
  return m_base;
}

void DocumentPlugin::playStartState()
{
  auto scenar = score::IDocument::try_get<Scenario::ScenarioDocumentModel>(
      this->m_context.document);
  if(!scenar)
    return;
  auto& sm = scenar->baseScenario().startState();

  Engine::score_to_ossia::play_state_from_ui(sm, this->context());
}

void DocumentPlugin::playStopState()
{
  auto scenar = score::IDocument::try_get<Scenario::ScenarioDocumentModel>(
      this->m_context.document);
  if(!scenar)
    return;
  auto& sm = scenar->baseScenario().endState();
  Engine::score_to_ossia::play_state_from_ui(sm, this->context());
}

bool DocumentPlugin::isPlaying() const
{
  if(m_base)
    return m_base->active();
  return false;
}

const ExecutionController& DocumentPlugin::executionController() const noexcept
{
  return m_context.app.guiApplicationPlugin<Engine::ApplicationPlugin>().execution();
}

std::shared_ptr<ossia::audio_protocol> DocumentPlugin::audioProto()
{
  auto dev = audio_device->sharedDevice();
  auto proto
      = &static_cast<ossia::audio_protocol&>(audio_device->getDevice()->get_protocol());

  return std::shared_ptr<ossia::audio_protocol>(dev, proto);
}

void DocumentPlugin::runAllCommands() const
{
  std::atomic_thread_fence(std::memory_order_seq_cst);
  ExecutionCommand com;
  while(m_ctxData->m_execQueue.try_dequeue(com))
    com();
}

void DocumentPlugin::registerAction(ExecutionAction& act)
{
  m_actions.push_back(&act);
}

void DocumentPlugin::slot_bench(ossia::bench_map b, int64_t ns)
{
  for(const auto& p : b)
  {
    if(p.second)
    {
      auto proc = m_ctxData->setupContext.proc_map.find(p.first);
      if(proc != m_ctxData->setupContext.proc_map.end())
      {
        if(proc->second)
        {
          const_cast<Process::ProcessModel*>(proc->second)
              ->benchmark(100. * *p.second / (double)ns);
        }
      }
    }
  }
}

void DocumentPlugin::on_deviceAdded(Device::DeviceInterface* dev)
{
  if(auto d = dev->getDevice())
  {
    connect(
        dev, &Device::DeviceInterface::deviceChanged, this,
        [this](ossia::net::device_base* old_dev, ossia::net::device_base* new_dev) {
      if(old_dev)
        unregisterDevice(old_dev);
      if(new_dev)
        registerDevice(new_dev);
        });
    registerDevice(d);
  }
}
}
