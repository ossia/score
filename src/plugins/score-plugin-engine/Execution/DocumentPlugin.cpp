// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "DocumentPlugin.hpp"

#include "BaseScenarioComponent.hpp"

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Scenario/Application/ScenarioActions.hpp>
#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <Scenario/Document/Event/EventExecution.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Interval/IntervalExecution.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/State/StateExecution.hpp>
#include <Scenario/Document/TimeSync/TimeSyncExecution.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Execution/score2OSSIA.hpp>

#include <Audio/AudioApplicationPlugin.hpp>
#include <Audio/AudioDevice.hpp>
#include <Audio/AudioTick.hpp>
#include <Audio/Settings/Model.hpp>
#include <Engine/ApplicationPlugin.hpp>
#include <Execution/DeviceAddresses.hpp>
#include <Execution/Settings/ExecutorModel.hpp>

#include <score/actions/ActionManager.hpp>
#include <score/model/ComponentUtils.hpp>
#include <score/plugins/documentdelegate/plugin/DocumentPlugin.hpp>
#include <score/tools/Bind.hpp>

#include <core/application/ApplicationSettings.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <ossia/audio/audio_protocol.hpp>
#include <ossia/dataflow/bench_map.hpp>
#include <ossia/detail/algorithms.hpp>
#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/for_each_port.hpp>
#include <ossia/dataflow/graph/graph_interface.hpp>
#include <ossia/dataflow/graph_edge.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/detail/flicks.hpp>
#include <ossia/detail/logger.hpp>
#include <ossia/editor/scenario/time_interval.hpp>
#include <ossia/network/common/path.hpp>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QTimer>

#include <atomic>
#include <thread>

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
      execGraph, execState
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
      onDeviceChanged(d, nullptr);
  });

  connect(
      this, &DocumentPlugin::finished, this, &DocumentPlugin::on_finished,
      Qt::DirectConnection);
}

void DocumentPlugin::recreateBase()
{
  m_base = std::make_shared<BaseScenarioElement>(m_ctxData->context, this);
  connect(
      m_base.get(), &Execution::BaseScenarioElement::finished, this,
      [this] {
    auto& app = context().doc.app;
    // The transport actions only exist with a GUI: headless (--no-gui),
    // looking up Actions::Stop throws out_of_range.
    if(app.applicationSettings.gui)
      app.actions.action<Actions::Stop>().action()->trigger();
    else
      app.guiApplicationPlugin<Engine::ApplicationPlugin>().execution().request_stop();
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

  if(audio_device)
  {
    if(auto devs = context().doc.findPlugin<Explorer::DeviceDocumentPlugin>())
    {
      const auto& rootNode = devs->explorer().rootNode();
      auto it = ossia::find_if(rootNode, [&](const Device::Node& val) {
        return val.is<Device::DeviceSettings>()
               && val.get<Device::DeviceSettings>().name == audio_device->name();
      });
      if(it != rootNode.end())
        devs->updateProxy.removeDevice(audio_device->settings());

      devs->list().setAudioDevice(nullptr);
    }
  }
  if(audio_device)
    delete audio_device;
  if(m_ctxData)
  {
    m_ctxData->context.alias.reset();
  }
}

// An edit command runs arbitrary UI code on behalf of the audio thread: one
// that fails must not unwind through the event loop (which aborts the
// application) nor starve the commands queued after it.
static void runEditCommand(ExecutionCommand& cmd) noexcept
{
  try
  {
    cmd();
  }
  catch(const std::exception& e)
  {
    qCritical() << "Execution: an edit command failed:" << e.what();
  }
  catch(...)
  {
    qCritical() << "Execution: an edit command failed (non-std exception)";
  }
}

void DocumentPlugin::ContextData::processEditCommands()
{
  ExecutionCommand cmd;
  GCCommand gc;
  bool ok = false;
  bool gc_ok = false;
  do
  {
    if((ok = m_editionQueue.try_dequeue(cmd)))
      runEditCommand(cmd);

    if((gc_ok = m_gcQueue.try_dequeue(gc)))
      gc();
  } while(ok || gc_ok);
}

void DocumentPlugin::processEditCommands()
{
  m_ctxData->processEditCommands();
}

void DocumentPlugin::on_finished()
{
  if(m_tid != -1)
  {
    killTimer(m_tid);
    m_tid = -1;
  }

  processEditCommands();
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
  auto& list
      = score::DocumentPlugin::context().plugin<Explorer::DeviceDocumentPlugin>().list();
  local_device = list.localDevice();
  if(audio_device)
    m_ctxData->execState->register_device(audio_device->getDevice());
  if(local_device)
    m_ctxData->execState->register_device(local_device->getDevice());
  for(auto dev : list.devices())
  {
    registerDevice(dev->getDevice());
  }
  m_ctxData->execState->apply_device_changes();
}

void DocumentPlugin::timerEvent(QTimerEvent* event)
{
  processEditCommands();
}

bool DocumentPlugin::registerDevice(ossia::net::device_base* d)
{
  if(!d)
    return false;
  if(m_ctxData->execState)
  {
    if(ossia::contains(m_ctxData->execState->edit_devices(), d))
      return false;
    m_ctxData->execState->register_device(d);

    if(m_base && m_base->active())
      d->get_protocol().start_execution();
    return true;
  }
  return false;
}

bool DocumentPlugin::unregisterDevice(ossia::net::device_base* d)
{
  if(!m_ctxData->execState)
    return false;
  if(!ossia::contains(m_ctxData->execState->edit_devices(), d))
    return false;

  m_ctxData->execState->unregister_device(d);

  // Ports keep raw pointers into the device tree and must be dropped from the
  // execution thread. The cleanup is queued, so by the time it runs this
  // device - and any other removed since - may be destroyed: snapshot the
  // addresses here and match on pointer identity below. One set for both kinds
  // also keeps the capture within the 128-byte ExecutionCommand budget.
  auto owned = deviceAddresses(*d);

  m_ctxData->context.executionQueue.enqueue(
      [wg = std::weak_ptr{m_ctxData->execGraph}, owned = std::move(owned)]() noexcept {
    if(auto g = wg.lock())
      clearAddresses(g->get_nodes(), owned);
  });
  return true;
}

void DocumentPlugin::updateDeviceExpressions()
{
  if(!m_base)
    return;

  auto& model = score::DocumentPlugin::context().document.model();
  for(auto ts : model.findChildren<Scenario::TimeSyncModel*>())
    if(auto c = score::findComponent<TimeSyncComponent>(ts->components()))
      if(c->OSSIATimeSync())
        c->updateTrigger();
  for(auto ev : model.findChildren<Scenario::EventModel*>())
    if(auto c = score::findComponent<EventComponent>(ev->components()))
      c->updateCondition();
}

void DocumentPlugin::waitForExecutionQueue()
{
  if(!m_base || !m_base->active())
    return;

  auto done = std::make_shared<std::atomic_bool>(false);
  m_ctxData->context.executionQueue.enqueue([done] { *done = true; });

  // The execution thread drains its queue on every tick. Nothing is waited on
  // from the audio thread, so this cannot hold it up.
  QElapsedTimer t;
  t.start();
  while(!*done && t.elapsed() < 1000)
  {
    std::this_thread::yield();
    processEditCommands();
  }
  if(!*done)
    qDebug() << "Device removal: the execution thread did not answer in time";
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
  {
    SCORE_SOFT_ASSERT(!execGraph); // "execGraph should always be unset here");
    execGraph->clear();
  }
  execGraph.reset();

  execState.reset();

  initExecState();

  execState->bufferSize = audiosettings.getBufferSize();
  execState->sampleRate = audiosettings.getRate();

  // Publish audio clock state for video frame pacing
  Audio::execution_samples.store(0, std::memory_order_relaxed);
  Audio::execution_sample_rate.store(audiosettings.getRate(), std::memory_order_relaxed);
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
  Transaction t{m_ctxData->context};
  for(auto& cable : model.cables)
  {
    m_ctxData->setupContext.connectCable(cable, t);
  }
  t.run_all();

  m_tid = startTimer(32);
  started();
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
    for(int i = 0; i < 100; i++)
    {
      processEditCommands();
      std::this_thread::yield();
    }
  }
  if(m_ctxData)
  {
    m_ctxData->m_created = false;
    if(m_ctxData)
      for(int i = 0; i < 100; i++)
      {
        processEditCommands();
        std::this_thread::yield();
        std::atomic_thread_fence(std::memory_order_seq_cst);
      }
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

  cleared();
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
  // Also for a device that is not connected yet: it gets its ossia device
  // through deviceChanged when it connects.
  connect(
      dev, &Device::DeviceInterface::deviceChanged, this,
      &DocumentPlugin::onDeviceChanged);
  // Most devices destroy their nodes before they say the device changed.
  connect(
      dev, &Device::DeviceInterface::deviceClearing, this,
      [this](ossia::net::device_base* d) { onDeviceChanged(d, nullptr); });
  // A device that comes back gets its nodes after it says it changed, and some
  // only learn their namespace later on.
  connect(
      dev, &Device::DeviceInterface::pathAdded, this,
      &DocumentPlugin::requestExpressionUpdate);
  connect(
      dev, &Device::DeviceInterface::namespaceUpdated, this,
      &DocumentPlugin::requestExpressionUpdate);
  if(auto d = dev->getDevice())
    onDeviceChanged(nullptr, d);
}

void DocumentPlugin::requestExpressionUpdate()
{
  if(!m_base || m_expressionUpdatePending)
    return;
  m_expressionUpdatePending = true;
  QTimer::singleShot(0, this, [this] {
    m_expressionUpdatePending = false;
    updateDeviceExpressions();
  });
}

void DocumentPlugin::onDeviceChanged(
    ossia::net::device_base* old_dev, ossia::net::device_base* new_dev)
{
  const bool removed = old_dev && unregisterDevice(old_dev);
  const bool added = new_dev && registerDevice(new_dev);

  // Triggers and conditions resolve their addresses once: those on this
  // device are false while it is gone, and work again when it comes back.
  if(removed)
  {
    updateDeviceExpressions();

    // The old device's nodes go away as soon as this returns: the ports and
    // expressions that point into them have to be let go of on the execution
    // thread first.
    waitForExecutionQueue();
  }
  else if(added)
  {
    requestExpressionUpdate();
  }
}
}
