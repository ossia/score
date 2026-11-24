#include "RecordSession.hpp"

#include <Process/Commands/Properties.hpp>
#include <Process/Dataflow/Cable.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/ProcessMimeSerialization.hpp>
#include <Process/Drop/ProcessDropHandler.hpp>
#include <Process/Drop/TakeDrop.hpp>
#include <Process/Execution/ProcessComponent.hpp>
#include <Process/Execution/PortTap.hpp>
#include <Process/Execution/TapLink.hpp>
#include <Process/ExecutionTransaction.hpp>
#include <Process/Process.hpp>
#include <Process/ExecutionFunctions.hpp>
#include <Process/TakeFiles.hpp>

#include <Device/Address/AddressSettings.hpp>
#include <Device/Node/DeviceNode.hpp>
#include <Device/Protocol/DeviceInterface.hpp>

#include <Explorer/DeviceList.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <ossia/network/base/node_functions.hpp>

#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Document/Interval/IntervalExecution.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <Engine/ApplicationPlugin.hpp>
#include <Execution/BaseScenarioComponent.hpp>
#include <Execution/DocumentPlugin.hpp>
#include <Execution/ExecutionController.hpp>
#include <Recording/Commands/PlaceTakes.hpp>
#include <Recording/Engine/CountIn.hpp>
#include <Recording/Takes/RecordedNotes.hpp>
#include <Recording/Takes/OverdubNotes.hpp>
#if SCORE_RECORDING_TEXTURES
#include <Gfx/GfxInputDevice.hpp>
#include <Recording/Video/TextureRecorder.hpp>
#endif

#include <ossia/audio/audio_parameter.hpp>
#include <Recording/Takes/Passes.hpp>
#include <Recording/Takes/RecordedCurves.hpp>
#include <Recording/Takes/RecordedValues.hpp>

#include <Automation/AutomationModel.hpp>
#include <Automation/Commands/SetAutomationMax.hpp>
#include <Curve/Commands/UpdateCurve.hpp>
#include <Curve/CurveModel.hpp>
#include <Curve/Settings/CurveSettingsModel.hpp>
#include <Midi/Commands/AddNote.hpp>
#include <Midi/Commands/RemoveNotes.hpp>
#include <Midi/MidiProcess.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/model/ComponentUtils.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/detail/algorithms.hpp>
#include <ossia/editor/scenario/time_interval.hpp>
#include <ossia/editor/scenario/time_process.hpp>
#include <ossia/network/common/destination_qualifiers.hpp>

#include <QFile>
#include <QFileInfo>
#include <QMimeData>
#include <QRegularExpression>
#include <QUrl>

#include <wobjectimpl.h>

#include <chrono>

W_OBJECT_IMPL(Recording::RecordSession)

namespace Recording
{
struct RecordSession::Source
{
  QPointer<const Process::Port> port;
  // Or an address
  State::Address address;
  State::Unit unit;
  //! Its value when the session started
  ossia::value initial;
  //! Received while waiting for playback; it starts the take
  std::optional<ossia::value> pending;

  Process::PortType type{};
  QPointer<Scenario::IntervalModel> box;
  QString prefix;

#if SCORE_RECORDING_TEXTURES
  //! A texture: its frames, and `values` as the clock that dates them
  std::unique_ptr<TextureRecorder> video;
#endif
  std::shared_ptr<AudioTake> audio;
  std::shared_ptr<MidiTake> midi;
  std::shared_ptr<ValueTake> values;
  std::shared_ptr<ossia::graph_node> node;
  std::unique_ptr<Execution::TapLink> link;

  // Writer thread, then GUI once it is joined
  WavStreamWriter wav;
  QString file;
  int rate{48000};
  std::vector<MidiTake::Event> events;
  std::vector<ValueTake::Event> valueEvents;

  int state() const noexcept
  {
    return audio  ? audio->state.load()
           : midi ? midi->state.load()
                  : values->state.load();
  }
  void requestStop() noexcept
  {
    if(audio)
      audio->stopRequested = true;
    if(midi)
      midi->stopRequested = true;
    if(values)
      values->stopRequested = true;
  }
};

namespace
{
//! "<process>-<port>", fit for a file name
QString prefixFor(const Process::Port& port)
{
  QString name;
  if(auto* proc = Process::parentProcess(&port))
    name = proc->metadata().getName();
  const QString port_name = port.visualName().isEmpty() ? port.name() : port.visualName();
  if(!port_name.isEmpty())
    name += (name.isEmpty() ? QString{} : QStringLiteral("-")) + port_name;
  if(name.isEmpty())
    name = QStringLiteral("Recording");
  static const QRegularExpression unsafe{QStringLiteral("[^A-Za-z0-9_.-]+")};
  name.replace(unsafe, QStringLiteral("_"));
  return name;
}

Execution::DocumentPlugin* executionOf(const score::DocumentContext& ctx)
{
  return ctx.findPlugin<Execution::DocumentPlugin>();
}

//! The parameter of an address in its device, on the GUI thread
ossia::net::parameter_base*
parameterOf(const score::DocumentContext& ctx, const State::Address& addr)
{
  auto* devices = ctx.findPlugin<Explorer::DeviceDocumentPlugin>();
  if(!devices)
    return nullptr;
  auto* dev = devices->list().findDevice(addr.device);
  auto* ossia_dev = dev ? dev->getDevice() : nullptr;
  if(!ossia_dev)
    return nullptr;
  auto* node = Device::findNodeFromPath(addr.path, *ossia_dev);
  return node ? node->get_parameter() : nullptr;
}

QString prefixFor(const State::Address& addr)
{
  QString name = addr.toString();
  static const QRegularExpression unsafe{QStringLiteral("[^A-Za-z0-9_.-]+")};
  name.replace(unsafe, QStringLiteral("_"));
  while(name.startsWith('_'))
    name.remove(0, 1);
  return name.isEmpty() ? QStringLiteral("Recording") : name;
}
}

RecordSession::RecordSession(const score::DocumentContext& ctx, RecordOptions options)
    : m_ctx{ctx}
    , m_options{options}
{
  m_poll.setInterval(20);
  connect(&m_poll, &QTimer::timeout, this, &RecordSession::poll);
  if(auto* c = ctx.app.findSettings<Curve::Settings::Model>())
  {
    m_curves.stepped = c->getCurveMode() == Curve::Settings::Mode::Parameter;
    m_curves.simplify = c->getSimplify();
    m_curves.simplificationRatio = c->getSimplificationRatio();
  }
}

RecordSession::~RecordSession()
{
  stopListening();
  if(m_running)
  {
    for(auto& s : m_sources)
      s->requestStop();
    m_writing = false;
    if(m_writer.joinable())
      m_writer.join();
    m_ctx.document.commandStack().enableActions();
  }
  QObject::disconnect(m_started);
  QObject::disconnect(m_finished);
}

bool RecordSession::addSource(const Process::Port& port)
{
  if(m_running)
    return false;
  const auto type = port.type();
  bool texture = false;
#if SCORE_RECORDING_TEXTURES
  texture = type == Process::PortType::Texture && TextureRecorder::canRecord(port);
#endif
  if(type != Process::PortType::Audio && type != Process::PortType::Midi
     && type != Process::PortType::Message && !texture)
    return false;

  auto s = std::make_unique<Source>();
  s->port = &port;
  s->type = type;
  s->box = Scenario::closestParentInterval(Process::parentProcess(&port));
  s->prefix = prefixFor(port);
  if(type == Process::PortType::Audio)
    s->audio = std::make_shared<AudioTake>(8, 1024, 512);
  else if(type == Process::PortType::Midi)
    s->midi = std::make_shared<MidiTake>(65536);
  else if(texture)
    s->values = std::make_shared<ValueTake>(16);
  else
    s->values = std::make_shared<ValueTake>(65536);
  m_sources.push_back(std::move(s));
  return true;
}

bool RecordSession::addAddress(const State::Address& address)
{
  if(m_running)
    return false;
  auto* devices = m_ctx.findPlugin<Explorer::DeviceDocumentPlugin>();
  if(!devices)
    return false;
  // The explorer's node, or the device's parameter when the explorer does
  // not show it
  auto* node = Device::try_getNodeFromAddress(devices->rootNode(), address);
  auto* param = parameterOf(m_ctx, address);
  if(!(node && node->is<Device::AddressSettings>()) && !param)
    return false;

  auto s = std::make_unique<Source>();
  s->address = address;
  if(node && node->is<Device::AddressSettings>())
  {
    const auto& settings = node->get<Device::AddressSettings>();
    s->unit = settings.unit;
    s->initial = settings.value;
  }
  else
  {
    s->unit = param->get_unit();
    s->initial = param->value();
  }
  s->prefix = prefixFor(address);
  // An audio input is recorded as sound, a camera as video, anything else as
  // its values
  if(dynamic_cast<ossia::audio_parameter*>(param))
  {
    s->type = Process::PortType::Audio;
    s->audio = std::make_shared<AudioTake>(8, 1024, 512);
  }
#if SCORE_RECORDING_TEXTURES
  else if(dynamic_cast<Gfx::texture_source_parameter*>(param))
  {
    s->type = Process::PortType::Texture;
    s->values = std::make_shared<ValueTake>(16);
  }
#endif
  else
  {
    s->type = Process::PortType::Message;
    s->values = std::make_shared<ValueTake>(65536);
  }
  m_sources.push_back(std::move(s));
  return true;
}

bool RecordSession::hasStreamAddresses() const noexcept
{
  return ossia::any_of(m_sources, [](const auto& s) {
    return !s->port
           && (s->type == Process::PortType::Audio || s->type == Process::PortType::Texture);
  });
}

void RecordSession::placeInNewBox(
    const Scenario::ProcessModel& scenario, Scenario::Point point,
    AddressPlacement placement, CurveOptions curves, bool playOnFirst)
{
  m_scenario = &scenario;
  m_point = point;
  m_placement = placement;
  m_curves = curves;
  m_playOnFirst = playOnFirst;
}

Scenario::IntervalModel* RecordSession::placedBox() const noexcept
{
  if(m_intoBox)
    return m_intoBox;
  if(!m_placedBox || !m_scenario)
    return nullptr;
  auto it = m_scenario->intervals.find(*m_placedBox);
  return it != m_scenario->intervals.end() ? &*it : nullptr;
}

bool RecordSession::waiting() const noexcept
{
  if(!m_running)
    return false;
  if(!m_recordersCreated)
    return true;
  for(const auto& s : m_sources)
    if(s->link && s->state() == AudioTake::Recording)
      return false;
  return true;
}

void RecordSession::placeInBox(
    Scenario::IntervalModel& box, AddressPlacement placement, CurveOptions curves)
{
  m_intoBox = &box;
  m_placement = placement;
  m_curves = curves;
  m_playOnFirst = false;
}

bool RecordSession::hasAddresses() const noexcept
{
  return ossia::any_of(m_sources, [](const auto& s) { return !s->port; });
}

bool RecordSession::start()
{
  if(m_running || m_sources.empty())
    return false;
  auto* exec = executionOf(m_ctx);
  if(!exec)
    return false;
  // Addresses are recorded on the clock of the box their scenario plays in
  if(hasAddresses())
  {
    if(!m_scenario && !m_intoBox)
      return false;
    auto* clock = m_intoBox ? m_intoBox.data()
                            : Scenario::closestParentInterval(m_scenario.data());
    if(!clock)
      return false;
    for(auto& s : m_sources)
      if(!s->port)
        s->box = clock;
  }

  m_running = true;
  // Undo and redo stay off: a command pushed meanwhile (live editing) would
  // turn them back on
  auto& stack = m_ctx.document.commandStack();
  stack.disableActions();
  m_undoGuard = connect(&stack, &score::CommandStack::canUndoChanged, this, [this](bool b) {
    if(b && m_running)
      m_ctx.document.commandStack().disableActions();
  }, Qt::QueuedConnection);
  m_redoGuard = connect(&stack, &score::CommandStack::canRedoChanged, this, [this](bool b) {
    if(b && m_running)
      m_ctx.document.commandStack().disableActions();
  }, Qt::QueuedConnection);

  // Counted in: a whole bar of clicks at least, then the next bar line
  const double startRate
      = m_options.countIn && m_options.startRate <= 0. ? 1. : m_options.startRate;
  const double lead = m_options.countIn ? 1. : 0.;
  for(auto& s : m_sources)
  {
    if(s->audio)
    {
      s->audio->startRate = startRate;
      s->audio->leadBars = lead;
      s->audio->length = m_options.length;
      s->audio->lengthBars = m_options.lengthBars;
    }
    if(s->midi)
    {
      s->midi->startRate = startRate;
      s->midi->leadBars = lead;
      s->midi->length = m_options.length;
      s->midi->lengthBars = m_options.lengthBars;
      s->midi->startOnFirstEvent = m_options.start == RecordOptions::Start::FirstEvent;
    }
    if(s->values)
    {
      s->values->startRate = startRate;
      s->values->leadBars = lead;
      s->values->length = m_options.length;
      s->values->lengthBars = m_options.lengthBars;
      // The takes of addresses start together, at the first value any of
      // them gets (listenToDevices) or with the playback
      s->values->startOnFirstEvent
          = m_options.start == RecordOptions::Start::FirstEvent && s->port;
    }
    // A texture's frames are taken from when it starts: its clock does not
    // wait for a grid or an event
    if(s->type == Process::PortType::Texture)
    {
      s->values->startRate = 0.;
      s->values->leadBars = 0.;
      s->values->startOnFirstEvent = false;
    }
  }

  // Playback ending ends the recording
  m_finished = connect(
      exec, &Execution::DocumentPlugin::finished, this, [this] { finish(); },
      Qt::QueuedConnection);

  if(exec->isPlaying())
  {
    createRecorders();
  }
  else
  {
    m_started = connect(
        exec, &Execution::DocumentPlugin::started, this,
        [this] { createRecorders(true); }, Qt::DirectConnection);
    if(m_options.start == RecordOptions::Start::FirstEvent && hasAddresses())
    {
      // A sound or a camera sends no first message: they start from the point
      if(hasStreamAddresses())
      {
        m_firstReceived = true;
        if(auto* engine = m_ctx.app.findGuiApplicationPlugin<Engine::ApplicationPlugin>())
          engine->execution().on_record(m_point.date);
      }
      else
      {
        listenToDevices();
      }
    }
    else if(m_options.start != RecordOptions::Start::OnPlay)
      if(auto* engine = m_ctx.app.findGuiApplicationPlugin<Engine::ApplicationPlugin>())
        engine->execution().request_play_global(true);
  }

  m_writing = true;
  m_writer = std::thread{[this] { writerLoop(); }};
  m_poll.start();
  return true;
}

void RecordSession::createRecorders(bool atStart)
{
  if(m_recordersCreated || !m_running)
    return;
  auto* exec = executionOf(m_ctx);
  // Linked as playback is set up, before its first tick: a take that starts
  // on the next bar from a stop starts on the first one
  if(!exec || !(exec->isPlaying() || (atStart && exec->baseScenario())))
    return;
  const auto& ectx = exec->context();
  const int rate = ectx.execState ? ectx.execState->sampleRate : 48000;

  for(auto& s : m_sources)
  {
    if(!s->box || (!s->port && s->address.device.isEmpty()))
      continue;
    auto* comp = boxComponent(*exec, *s->box);
    if(!comp || !comp->OSSIAInterval())
      continue;
    auto clock = comp->OSSIAInterval()->node;

    s->rate = rate;
    if(s->audio)
      s->node = std::make_shared<audio_record_node>(s->audio, clock);
    else if(s->midi)
      s->node = std::make_shared<midi_record_node>(s->midi, clock);
    else
      s->node = std::make_shared<value_record_node>(s->values, clock);

    if(s->type == Process::PortType::Texture)
    {
#if SCORE_RECORDING_TEXTURES
      // The clock alone; the frames come through the graphics context
      s->link = std::make_unique<Execution::TapLink>(ectx, s->node);
      const QString folder = Process::takesFolder(m_ctx);
      const int n = Process::reserveTakeNumber(folder, s->prefix, [&](int k) {
        return Process::takeFile(folder, s->prefix, k, m_options.video.extension());
      });
      s->file = Process::takeFile(folder, s->prefix, n, m_options.video.extension());
      if(s->port)
      {
        s->video = std::make_unique<TextureRecorder>(
            *s->port, m_ctx, s->file, m_options.video);
      }
      else
      {
        // A texture input of a device: the node that renders it
        auto producer = [&ctx = m_ctx, addr = s->address]() -> ossia::gfx::port_index {
          if(auto* p = dynamic_cast<Gfx::texture_source_parameter*>(parameterOf(ctx, addr)))
            return p->source();
          return {-1, -1};
        };
        s->video = std::make_unique<TextureRecorder>(
            std::move(producer), m_ctx, s->file, m_options.video);
      }
      if(!s->video->start())
      {
        s->video.reset();
        s->link.reset();
      }
#endif
    }
    else if(s->port)
    {
      s->link = std::make_unique<Execution::TapLink>(*s->port, ectx, s->node);
    }
    else if(ectx.execState)
    {
      auto dest = Execution::makeDestination(
          *ectx.execState, State::AddressAccessor{s->address});
      if(dest)
        s->link = std::make_unique<Execution::TapLink>(dest->address(), ectx, s->node);
    }
    if(!s->link || !s->link->linked())
    {
      s->link.reset();
      s->node.reset();
      continue;
    }
    if(s->audio)
      s->audio->startRequested = true;
    else if(s->midi)
      s->midi->startRequested = true;
    else
      s->values->startRequested = true;
  }
  m_recordersCreated = true;
  // The engine records them from now on
  stopListening();

  if(m_options.countIn)
  {
    std::shared_ptr<ossia::graph_node> clock;
    std::vector<std::shared_ptr<AudioTake>> audio;
    std::vector<std::shared_ptr<MidiTake>> midi;
    std::vector<std::shared_ptr<ValueTake>> values;
    for(auto& s : m_sources)
    {
      if(!s->link)
        continue;
      if(!clock)
        if(auto* comp = boxComponent(*exec, *s->box); comp && comp->OSSIAInterval())
          clock = comp->OSSIAInterval()->node;
      if(s->audio)
        audio.push_back(s->audio);
      if(s->midi)
        midi.push_back(s->midi);
      if(s->values)
        values.push_back(s->values);
    }
    // The main output of the document's audio device
    ossia::net::parameter_base* param{};
    if(auto* devices = m_ctx.findPlugin<Explorer::DeviceDocumentPlugin>())
      if(auto* audio = devices->list().audioDevice(); audio && audio->getDevice())
        if(auto* out
           = ossia::net::find_node(audio->getDevice()->get_root_node(), "/out/main"))
          param = out->get_parameter();
    if(!clock || !param || !Execution::TapTicker::instance())
      m_warnings.push_back(
          tr("The count-in could not click: %1")
              .arg(!param ? tr("the audio device has no main output")
                          : tr("what is recorded does not play")));
    if(clock && param && Execution::TapTicker::instance())
    {
      // Reads atomics only: called on the audio thread
      auto waiting = [audio, midi, values] {
        for(auto& t : audio)
          if(t->state.load(std::memory_order_relaxed) < AudioTake::Recording)
            return true;
        for(auto& t : midi)
          if(t->state.load(std::memory_order_relaxed) < MidiTake::Recording)
            return true;
        for(auto& t : values)
          if(t->state.load(std::memory_order_relaxed) < ValueTake::Recording)
            return true;
        return false;
      };
      auto node = std::make_shared<count_in_node>(rate, clock, std::move(waiting));
      m_countIn = node;
      ectx.executionQueue.enqueue([g = ectx.execGraph, ws = std::weak_ptr{ectx.execState},
                                   node, param] {
        g->add_node(node);
        node->metro.audio_out.address = param;
        if(auto st = ws.lock())
          st->register_port(node->metro.audio_out);
        g->mark_dirty();
      });
      Execution::TapTicker::instance()->add(node);
    }
  }

  // Recorded into an automation: it stops writing while the take records
  // (latch), only in the execution
  if(m_options.target == RecordOptions::Target::Into && m_options.into
     && qobject_cast<const Automation::ProcessModel*>(m_options.into.data()))
    if(auto* comp = score::findComponent<Execution::ProcessComponent>(
           m_options.into->components()))
      if((m_latched = comp->OSSIAProcessPtr()))
        ectx.executionQueue.enqueue([p = m_latched] { p->mute(true); });
}

int RecordSession::countInBeats() const noexcept
{
  if(auto* n = static_cast<count_in_node*>(m_countIn.get()))
    return n->beats.load(std::memory_order_relaxed);
  return m_countInBeats;
}

void RecordSession::removeCountIn()
{
  if(!m_countIn)
    return;
  m_countInBeats = static_cast<count_in_node&>(*m_countIn).beats.load();
  if(auto* ticker = Execution::TapTicker::instance())
    ticker->remove(m_countIn.get());
  auto* exec = executionOf(m_ctx);
  if(exec && exec->isPlaying())
  {
    const auto& ectx = exec->context();
    ectx.executionQueue.enqueue([g = std::weak_ptr{ectx.execGraph},
                                 ws = std::weak_ptr{ectx.execState},
                                 gcq = ectx.weakGCQueue(), node = std::move(m_countIn)]() mutable {
      auto& out = static_cast<count_in_node&>(*node).metro.audio_out;
      if(auto st = ws.lock())
        st->unregister_port(out);
      if(auto gr = g.lock())
      {
        gr->remove_node(node);
        gr->mark_dirty();
      }
      // Freed on the GUI thread
      if(auto q = gcq.lock())
        q->enqueue(Execution::gc(std::move(node)));
    });
  }
  m_countIn.reset();
}

void RecordSession::listenToDevices()
{
  auto* devices = m_ctx.findPlugin<Explorer::DeviceDocumentPlugin>();
  if(!devices)
    return;
  for(auto& s : m_sources)
  {
    if(s->port)
      continue;
    auto* dev = devices->list().findDevice(s->address.device);
    if(!dev || !dev->connected())
      continue;
    const auto requested = dev->listeningRequests();
    if(!ossia::contains(requested, s->address))
    {
      // Only what comes from now on: the value it holds does not start a take
      dev->setListening(s->address, true, false);
      m_listeningAdded.push_back(s->address);
    }
    if(!ossia::contains(m_listened, dev))
    {
      m_listened.push_back(dev);
      dev->valueUpdated.connect<&RecordSession::deviceValue>(*this);
    }
  }
}

void RecordSession::stopListening()
{
  for(auto& dev : m_listened)
  {
    if(!dev)
      continue;
    dev->valueUpdated.disconnect<&RecordSession::deviceValue>(*this);
    for(const auto& addr : m_listeningAdded)
      if(addr.device == dev->name())
        dev->setListening(addr, false);
  }
  m_listened.clear();
  m_listeningAdded.clear();
}

void RecordSession::deviceValue(const State::Address& addr, const ossia::value& v)
{
  // From the device's thread
  QMetaObject::invokeMethod(
      this, [this, addr, v] { firstValue(addr, v); }, Qt::QueuedConnection);
}

void RecordSession::firstValue(const State::Address& addr, const ossia::value& v)
{
  if(!m_running || m_recordersCreated)
    return;
  bool ours = false;
  for(auto& s : m_sources)
    if(!s->port && s->address == addr)
    {
      s->pending = v;
      ours = true;
    }
  if(!ours || m_firstReceived)
    return;
  m_firstReceived = true;
  if(m_playOnFirst)
    if(auto* engine = m_ctx.app.findGuiApplicationPlugin<Engine::ApplicationPlugin>())
      engine->execution().on_record(m_point.date);
}

void RecordSession::drain()
{
  std::lock_guard _{m_drainLock};
  for(auto& s : m_sources)
  {
    if(s->audio)
    {
      s->audio->drain([&](AudioTake::Block& b) {
        if(!s->wav.isOpen() && !s->wav.failed())
        {
          const QString folder = Process::takesFolder(m_ctx);
          const int n = Process::reserveTakeNumber(folder, s->prefix, [&](int k) {
            return Process::takeFile(folder, s->prefix, k, QStringLiteral("wav"));
          });
          s->file = Process::takeFile(folder, s->prefix, n, QStringLiteral("wav"));
          s->wav.open(s->file.toStdString(), b.channels, s->rate, m_options.audioFormat);
        }
        if(!s->wav.isOpen())
          return;
        const float* ptrs[8]{};
        for(int c = 0; c < b.channels && c < 8; c++)
          ptrs[c] = b.data.data() + std::size_t(c) * s->audio->blockFrames;
        s->wav.write(ptrs, b.frames);
      });
    }
    if(s->midi)
    {
      MidiTake::Event ev;
      while(s->midi->queue.try_dequeue(ev))
        s->events.push_back(ev);
    }
    if(s->values)
    {
      ValueTake::Event ev;
      while(s->values->queue.try_dequeue(ev))
        s->valueEvents.push_back(std::move(ev));
    }
  }
}

void RecordSession::writerLoop()
{
  using namespace std::chrono_literals;
  while(m_writing)
  {
    drain();
    std::this_thread::sleep_for(5ms);
  }
}

void RecordSession::stop()
{
  if(!m_running)
    return;
  for(auto& s : m_sources)
    s->requestStop();
  auto* exec = executionOf(m_ctx);
  if(!m_recordersCreated || !exec || !exec->isPlaying())
    finish();
  // Else the recorders end on their next tick, which poll() sees
}

void RecordSession::poll()
{
  if(!m_running)
    return;
  // Playback asked for, or awaited: the recorders are made once it runs
  // (DocumentPlugin::started comes before the base scenario is active).
  if(!m_recordersCreated)
  {
    createRecorders();
    return;
  }
  for(auto& s : m_sources)
  {
    if(!s->link)
      continue;
    if(s->state() != AudioTake::Done)
      return;
  }
  finish();
}

void RecordSession::finish()
{
  if(!m_running)
    return;
  m_running = false;
  m_poll.stop();
  QObject::disconnect(m_started);
  QObject::disconnect(m_finished);
  QObject::disconnect(m_undoGuard);
  QObject::disconnect(m_redoGuard);
  stopListening();

  removeCountIn();
  auto* exec = executionOf(m_ctx);
  const bool engine_over = !exec || !exec->isPlaying();
  if(m_latched)
  {
    if(!engine_over)
      exec->context().executionQueue.enqueue(
          [p = m_latched, b = m_options.into && m_options.into->muted()] { p->mute(b); });
    m_latched.reset();
  }
  for(auto& s : m_sources)
  {
    // The block being filled is the node's until it stops running
    if(engine_over && s->audio && s->node)
      static_cast<audio_record_node&>(*s->node).flushStopped();
    s->link.reset();
  }

#if SCORE_RECORDING_TEXTURES
  for(auto& s : m_sources)
    if(s->video)
      s->video->stop();
#endif
  m_writing = false;
  if(m_writer.joinable())
    m_writer.join();
  drain();
  for(auto& s : m_sources)
    if(s->wav.isOpen())
      s->wav.close();

  place();
  m_ctx.document.commandStack().enableActions();
  finished();
}

Process::ProcessModel* RecordSession::createTake(
    Scenario::Command::Macro& macro, const Scenario::IntervalModel& box,
    const Process::ProcessData& data)
{
  if(!m_options.nodePosition)
    return macro.createProcessInNewSlot(box, data);
  return macro.createProcessInNewSlot(
      box, data, freeNodePosition(box, *m_options.nodePosition));
}

void RecordSession::connectToInlet(
    Scenario::Command::Macro& macro, const Process::ProcessModel& take,
    const Process::Port& port)
{
  auto* inlet = qobject_cast<const Process::Inlet*>(&port);
  if(!inlet)
    return;
  auto* doc = score::IDocument::try_get<Scenario::ScenarioDocumentModel>(m_ctx.document);
  if(!doc)
    return;
  for(auto* out : take.outlets())
  {
    if(out->type() != inlet->type())
      continue;
    macro.createCable(*doc, *out, *inlet, Process::CableType::ImmediateGlutton);
    return;
  }
}

bool RecordSession::placePasses(
    Source& s, Take& t, std::optional<Scenario::Command::Macro>& macro, double bars,
    bool oneLoop)
{
  constexpr int64_t flicks_per_quarter = 352800000;
  const int num = s.audio ? s.audio->meterNumerator.load()
                  : s.midi ? s.midi->meterNumerator.load()
                           : s.values->meterNumerator.load();
  const int den = s.audio ? s.audio->meterDenominator.load()
                  : s.midi ? s.midi->meterDenominator.load()
                           : s.values->meterDenominator.load();
  const double quarters_per_bar = (num > 0 && den > 0) ? 4. * num / den : 4.;
  const int64_t length
      = int64_t(std::llround(bars * quarters_per_bar * flicks_per_quarter));
  if(length <= 0)
    return false;
  using P = RecordOptions::Passes;
  std::vector<Pass> passes;
  bool loops = true;
  if(oneLoop)
  {
    // Exactly the length asked: where the take falls short of it, silence
    passes.push_back({t.startDate, t.startDate + length});
  }
  else
  {
    passes = passesOf(t.startDate, t.endDate, length);
    loops = m_options.passes != P::SideBySide;
    if(m_options.passes == P::LastOnly && !passes.empty())
      passes.erase(passes.begin(), passes.end() - 1);
  }
  if(passes.empty())
    return false;

  const QFileInfo full{t.file};
  const QString ext = full.suffix();
  const auto& handlers = m_ctx.app.interfaces<Process::ProcessDropHandlerList>();
  const double tempo = s.audio ? s.audio->tempo.load()
                       : s.midi ? s.midi->tempo.load()
                                : s.values->tempo.load();
  constexpr double flicks_per_second = 705600000.;
  const auto frame_of = [&](int64_t flicks) {
    // Frames run in real time, dates in the box's: they part at another tempo
    const double ratio = tempo > 0. ? 120. / tempo : 1.;
    return int64_t(std::llround(double(flicks) / flicks_per_second * s.rate * ratio));
  };
  auto notes = s.midi ? notesOf(s.events, t.endDate) : std::vector<RecordedNote>{};
  quantizeNotes(notes, m_options.midiQuantize);

  bool placed = false;
  bool first = true;
  for(std::size_t k = 0; k < passes.size(); k++)
  {
    const Pass& p = passes[k];
    const int64_t len = p.end - p.start;
    // A loop plays its content round from the start of the box: turned so
    // that each moment of the box hears what was played there
    const int64_t rotate = loops ? ((p.start % len) + len) % len : 0;
    const QString file = full.absolutePath() + "/" + full.completeBaseName()
                         + QStringLiteral("-pass%1.").arg(k + 1) + ext;
    bool ok = false;
    if(s.audio)
    {
      ok = writeWavSegment(
          t.file, file, frame_of(p.start - t.startDate), frame_of(len),
          frame_of(rotate), m_options.audioFormat);
    }
    else if(s.midi)
    {
      const auto pn = notesOfPass(notes, p, rotate);
      const auto smf = smfOf(pn, 0, len, tempo);
      QFile f{file};
      ok = f.open(QIODevice::WriteOnly)
           && f.write(smf.data(), qint64(smf.size())) == qint64(smf.size());
    }
    else
    {
      const auto pv = valuesOfPass(s.valueEvents, p, rotate);
      const auto json = valueTakeJson(
          pv, 0, len, s.rate, tempo, s.values->meterNumerator.load(),
          s.values->meterDenominator.load());
      QFile f{file};
      ok = f.open(QIODevice::WriteOnly) && f.write(json) == json.size();
    }
    if(!ok)
      continue;

    QMimeData mime;
    mime.setUrls({QUrl::fromLocalFile(file)});
    if(loops)
    {
      const auto meta = Process::encode_clip_metadata(
          {file.toStdString()}, halp::clip_stretch::automatic, true);
      mime.setData(
          QString::fromUtf8(
              Process::clip_metadata_mime.data(),
              qsizetype(Process::clip_metadata_mime.size())),
          QByteArray::fromStdString(meta));
    }
    auto drops = handlers.getDrop(mime, m_ctx);
    if(drops.empty())
      continue;
    if(!macro)
      macro.emplace(new PlaceTakes, m_ctx);
    score::Dispatcher_T disp{*macro};
    for(auto& d : drops)
    {
      // Alternatives share one slot
      Process::ProcessModel* proc{};
      if(loops && !first)
      {
        proc = macro->createProcess(*s.box, d.creation, {});
        if(proc)
          macro->addLayerToLastSlot(*s.box, *proc);
      }
      else
      {
        proc = createTake(*macro, *s.box, d.creation);
      }
      if(!proc)
        continue;
      if(s.port)
        connectToInlet(*macro, *proc, *s.port);
      if(d.setup)
        d.setup(*proc, disp);
      if(loops)
      {
        Process::loop_clip(*proc, TimeVal{len}, disp);
        if(proc->startOffset() != TimeVal::zero())
          macro->submit(new Process::SetStartOffset{*proc, TimeVal::zero()});
        // The last pass plays
        if(k + 1 < passes.size())
          macro->submit(new Process::SetProcessMuted{*proc, true});
      }
      else
      {
        macro->submit(new Process::SetLoop{*proc, false});
        macro->submit(new Process::SetStartOffset{*proc, TimeVal{-p.start}});
      }
      t.placed = proc;
      m_placed.push_back(proc);
      placed = true;
      first = false;
    }
  }
  return placed;
}

void RecordSession::place()
{
  m_takes.clear();
  const auto& handlers = m_ctx.app.interfaces<Process::ProcessDropHandlerList>();
  std::optional<Scenario::Command::Macro> macro;
  bool placed_any = false;

  for(auto& s : m_sources)
  {
    if(!s->port)
      continue;
    Take t;
    t.box = s->box;
    t.midi = bool(s->midi);
    t.values = bool(s->values);
    if(s->audio)
    {
      if(s->file.isEmpty() || s->wav.frames() == 0)
        continue;
      t.file = s->file;
      t.startDate = s->audio->startDate;
      t.endDate = s->audio->endDate;
    }
    else if(s->type == Process::PortType::Texture)
    {
#if SCORE_RECORDING_TEXTURES
      if(!s->video || s->video->frames() == 0 || !QFile::exists(s->file))
        continue;
      t.file = s->file;
      t.startDate = s->values->startDate;
      t.endDate = s->values->endDate;
#else
      continue;
#endif
    }
    else if(s->values)
    {
      if(s->valueEvents.empty())
        continue;
      t.startDate = s->values->startDate;
      t.endDate = s->values->endDate;
      const QString folder = Process::takesFolder(m_ctx);
      const int n = Process::reserveTakeNumber(folder, s->prefix, [&](int k) {
        return Process::takeFile(folder, s->prefix, k, QStringLiteral("values"));
      });
      t.file = Process::takeFile(folder, s->prefix, n, QStringLiteral("values"));
      const auto json = valueTakeJson(
          s->valueEvents, t.startDate, t.endDate, s->rate, s->values->tempo.load(),
          s->values->meterNumerator.load(), s->values->meterDenominator.load());
      QFile f{t.file};
      if(!f.open(QIODevice::WriteOnly) || f.write(json) != json.size())
        continue;

      // Into an automation: its curve over the span becomes the values
      if(m_options.target == RecordOptions::Target::Into)
      {
        if(auto* into = qobject_cast<const Automation::ProcessModel*>(m_options.into.data()))
        {
          const auto& acc = into->address().qualifiers.get().accessors;
          const int component = acc.empty() ? 0 : acc.front();
          if(auto c = spliceCurve(
                 *into, s->valueEvents, component, t.startDate, t.endDate, m_curves))
          {
            if(!macro)
              macro.emplace(new PlaceTakes, m_ctx);
            auto& autom = const_cast<Automation::ProcessModel&>(*into);
            macro->submit(new Automation::SetMin{autom, c->min});
            macro->submit(new Automation::SetMax{autom, c->max});
            macro->submit(new Curve::UpdateCurve{autom.curve(), std::move(c->segments)});
            t.placed = &autom;
            placed_any = true;
          }
        }
        m_takes.push_back(std::move(t));
        continue;
      }
    }
    else
    {
      if(s->events.empty())
        continue;
      t.startDate = s->midi->startDate;
      t.endDate = s->midi->endDate;
      auto notes = notesOf(s->events, t.endDate);
      quantizeNotes(notes, m_options.midiQuantize);
      if(notes.empty())
        continue;
      const QString folder = Process::takesFolder(m_ctx);
      const int n = Process::reserveTakeNumber(folder, s->prefix, [&](int k) {
        return Process::takeFile(folder, s->prefix, k, QStringLiteral("mid"));
      });
      t.file = Process::takeFile(folder, s->prefix, n, QStringLiteral("mid"));
      const auto smf
          = smfOf(notes, t.startDate, t.endDate - t.startDate, s->midi->tempo.load());
      QFile f{t.file};
      if(!f.open(QIODevice::WriteOnly)
         || f.write(smf.data(), qint64(smf.size())) != qint64(smf.size()))
        continue;

      // Into a piano roll: its notes change, the file stays in Takes
      if(m_options.target == RecordOptions::Target::Into)
      {
        if(auto* into = qobject_cast<const Midi::ProcessModel*>(m_options.into.data()))
        {
          auto od = overdubNotes(*into, notes, t.startDate, t.endDate, m_options.replace);
          if(!macro)
            macro.emplace(new PlaceTakes, m_ctx);
          if(!od.remove.empty())
            macro->submit(new Midi::RemoveNotes{*into, od.remove});
          if(!od.add.empty())
            macro->submit(new Midi::AddNotes{*into, od.add});
          t.placed = const_cast<Midi::ProcessModel*>(into);
          placed_any = true;
        }
        m_takes.push_back(std::move(t));
        continue;
      }
    }

    // Passes: a file and a process for each
    const bool fixed_loop = m_options.passes == RecordOptions::Passes::One
                            && m_options.loopFixedLength && m_options.lengthBars > 0.
                            && m_options.target == RecordOptions::Target::ThisBox;
    if((m_options.passes != RecordOptions::Passes::One || fixed_loop) && s->box
       && m_options.target != RecordOptions::Target::FileOnly
       && s->type != Process::PortType::Texture)
    {
      const double bars = fixed_loop ? m_options.lengthBars : m_options.passBars;
      if(placePasses(*s, t, macro, bars, fixed_loop))
        placed_any = true;
      m_takes.push_back(std::move(t));
      continue;
    }

    if((m_options.target == RecordOptions::Target::ThisBox
        || m_options.target == RecordOptions::Target::Into)
       && s->box)
    {
      QMimeData mime;
      mime.setUrls({QUrl::fromLocalFile(t.file)});
      auto drops = handlers.getDrop(mime, m_ctx);
      if(!drops.empty())
      {
        if(!macro)
          macro.emplace(new PlaceTakes, m_ctx);
        score::Dispatcher_T disp{*macro};
        for(auto& d : drops)
        {
          auto* p = createTake(*macro, *s->box, d.creation);
          if(!p)
            continue;
          if(d.setup)
            d.setup(*p, disp);
          connectToInlet(*macro, *p, *s->port);
          // The take starts where the recording started in the box
          macro->submit(new Process::SetLoop{*p, false});
          macro->submit(new Process::SetStartOffset{*p, TimeVal{-t.startDate}});
          t.placed = p;
          m_placed.push_back(p);
          placed_any = true;
        }
      }
    }
    m_takes.push_back(std::move(t));
  }

  // What was recorded is heard from its takes now
  if(m_options.muteSource && placed_any && macro)
  {
    std::vector<const Process::ProcessModel*> sources;
    // The takes now feed the inlets they were recorded from too: they play
    const auto isTake = [&](const Process::ProcessModel* proc) {
      return ossia::any_of(m_placed, [=](const auto& t) { return t == proc; });
    };
    const auto add = [&](const Process::ProcessModel* proc) {
      if(proc && !proc->muted() && !isTake(proc) && !ossia::contains(sources, proc))
        sources.push_back(proc);
    };
    for(auto& s : m_sources)
    {
      if(!s->port)
        continue;
      // An inlet's take plays into it: what fed it is muted, not the process
      if(qobject_cast<const Process::Inlet*>(s->port.data()))
      {
        for(const auto& path : s->port->cables())
          if(auto* cable = path.try_find(m_ctx))
            if(auto* src = cable->source().try_find(m_ctx))
              add(Process::parentProcess(src));
      }
      else
      {
        add(Process::parentProcess(s->port.data()));
      }
    }
    for(auto* proc : sources)
      macro->submit(new Process::SetProcessMuted{*proc, true});
  }

  if(hasAddresses() && (m_scenario || m_intoBox))
  {
    std::vector<AddressTake> takes;
    std::vector<FileTake> files;
    for(auto& s : m_sources)
    {
      if(s->port)
        continue;
      // An audio or texture input: its file, in the same box
      if(s->type == Process::PortType::Audio || s->type == Process::PortType::Texture)
      {
        FileTake f;
        if(s->audio)
        {
          if(s->file.isEmpty() || s->wav.frames() == 0)
            continue;
          f.startDate = s->audio->startDate;
          f.endDate = s->audio->endDate;
        }
        else
        {
#if SCORE_RECORDING_TEXTURES
          if(!s->video || s->video->frames() == 0 || !QFile::exists(s->file))
            continue;
          f.startDate = s->values->startDate;
          f.endDate = s->values->endDate;
#else
          continue;
#endif
        }
        f.file = s->file;
        Take r;
        r.box = s->box;
        r.file = f.file;
        r.startDate = f.startDate;
        r.endDate = f.endDate;
        m_takes.push_back(r);
        files.push_back(std::move(f));
        continue;
      }
      AddressTake t;
      t.address = s->address;
      t.unit = s->unit;
      t.startDate = s->values->startDate;
      t.endDate = s->values->endDate;
      // What it got while playback started, else what it held: from the start
      if(s->pending || !s->valueEvents.empty())
      {
        t.events.push_back({t.startDate, s->pending ? *s->pending : s->initial});
        for(auto& e : s->valueEvents)
          t.events.push_back(std::move(e));
      }
      // An address that got nothing is not placed
      if(!s->pending && s->valueEvents.empty())
        t.events.clear();
      if(t.events.empty())
        continue;
      Take r;
      r.box = s->box;
      r.values = true;
      r.startDate = t.startDate;
      r.endDate = t.endDate;
      m_takes.push_back(r);
      takes.push_back(std::move(t));
    }
    if(!takes.empty() || !files.empty())
    {
      if(!macro)
        macro.emplace(new PlaceTakes, m_ctx);
      if(m_intoBox)
      {
        if(placeAddressTakesInBox(
               *macro, *m_intoBox, takes, m_placement, m_curves, files,
               m_options.nodePosition))
          placed_any = true;
      }
      else if(auto* box = placeAddressTakes(
                  *macro, *m_scenario, m_point.y, takes, m_placement, m_curves, files))
      {
        m_placedBox = box->id();
        placed_any = true;
      }
    }
  }

  if(macro && placed_any)
    macro->commit();
}
}
