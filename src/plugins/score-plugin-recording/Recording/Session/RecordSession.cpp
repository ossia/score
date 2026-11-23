#include "RecordSession.hpp"

#include <Process/Commands/Properties.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Drop/ProcessDropHandler.hpp>
#include <Process/Execution/TapLink.hpp>
#include <Process/Process.hpp>
#include <Process/ExecutionFunctions.hpp>
#include <Process/TakeFiles.hpp>

#include <Device/Address/AddressSettings.hpp>
#include <Device/Node/DeviceNode.hpp>
#include <Device/Protocol/DeviceInterface.hpp>

#include <Explorer/DeviceList.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Document/Interval/IntervalExecution.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <Engine/ApplicationPlugin.hpp>
#include <Execution/BaseScenarioComponent.hpp>
#include <Execution/DocumentPlugin.hpp>
#include <Execution/ExecutionController.hpp>
#include <Recording/Commands/PlaceTakes.hpp>
#include <Recording/Takes/RecordedNotes.hpp>
#include <Recording/Takes/RecordedValues.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/model/ComponentUtils.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/detail/algorithms.hpp>
#include <ossia/editor/scenario/time_interval.hpp>

#include <QFile>
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
  if(type != Process::PortType::Audio && type != Process::PortType::Midi
     && type != Process::PortType::Message)
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
  auto* node = Device::try_getNodeFromAddress(devices->rootNode(), address);
  if(!node || !node->is<Device::AddressSettings>())
    return false;
  const auto& settings = node->get<Device::AddressSettings>();

  auto s = std::make_unique<Source>();
  s->address = address;
  s->unit = settings.unit;
  s->initial = settings.value;
  s->type = Process::PortType::Message;
  s->prefix = prefixFor(address);
  s->values = std::make_shared<ValueTake>(65536);
  m_sources.push_back(std::move(s));
  return true;
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
    if(!m_scenario)
      return false;
    auto* clock = Scenario::closestParentInterval(m_scenario.data());
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

  for(auto& s : m_sources)
  {
    if(s->audio)
    {
      s->audio->startRate = m_options.startRate;
      s->audio->length = m_options.length;
      s->audio->lengthBars = m_options.lengthBars;
    }
    if(s->midi)
    {
      s->midi->startRate = m_options.startRate;
      s->midi->length = m_options.length;
      s->midi->lengthBars = m_options.lengthBars;
      s->midi->startOnFirstEvent = m_options.start == RecordOptions::Start::FirstEvent;
    }
    if(s->values)
    {
      s->values->startRate = m_options.startRate;
      s->values->length = m_options.length;
      s->values->lengthBars = m_options.lengthBars;
      // The takes of addresses start together, at the first value any of
      // them gets (listenToDevices) or with the playback
      s->values->startOnFirstEvent
          = m_options.start == RecordOptions::Start::FirstEvent && s->port;
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
        [this] { createRecorders(); }, Qt::QueuedConnection);
    if(m_options.start == RecordOptions::Start::FirstEvent && hasAddresses())
      listenToDevices();
    else if(m_options.start != RecordOptions::Start::OnPlay)
      if(auto* engine = m_ctx.app.findGuiApplicationPlugin<Engine::ApplicationPlugin>())
        engine->execution().request_play_global(true);
  }

  m_writing = true;
  m_writer = std::thread{[this] { writerLoop(); }};
  m_poll.start();
  return true;
}

void RecordSession::createRecorders()
{
  if(m_recordersCreated || !m_running)
    return;
  auto* exec = executionOf(m_ctx);
  if(!exec || !exec->isPlaying())
    return;
  const auto& ectx = exec->context();
  const int rate = ectx.execState ? ectx.execState->sampleRate : 48000;

  for(auto& s : m_sources)
  {
    if(!s->box || (!s->port && s->address.device.isEmpty()))
      continue;
    auto* comp = score::findComponent<Execution::IntervalComponent>(s->box->components());
    // The base interval's is the document's execution's, not one of its components
    if(!comp)
      if(const auto& base = exec->baseScenario();
         base && &base->baseInterval().interval() == s->box)
        comp = &base->baseInterval();
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

    if(s->port)
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

  auto* exec = executionOf(m_ctx);
  const bool engine_over = !exec || !exec->isPlaying();
  for(auto& s : m_sources)
  {
    // The block being filled is the node's until it stops running
    if(engine_over && s->audio && s->node)
      static_cast<audio_record_node&>(*s->node).flushStopped();
    s->link.reset();
  }

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
    }
    else
    {
      if(s->events.empty())
        continue;
      t.startDate = s->midi->startDate;
      t.endDate = s->midi->endDate;
      const auto notes = notesOf(s->events, t.endDate);
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
    }

    if(m_options.target == RecordOptions::Target::ThisBox && s->box)
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
          auto* p = macro->createProcessInNewSlot(*s->box, d.creation);
          if(!p)
            continue;
          if(d.setup)
            d.setup(*p, disp);
          // The take starts where the recording started in the box
          macro->submit(new Process::SetLoop{*p, false});
          macro->submit(new Process::SetStartOffset{*p, TimeVal{-t.startDate}});
          t.placed = p;
          placed_any = true;
        }
      }
    }
    m_takes.push_back(std::move(t));
  }

  if(hasAddresses() && m_scenario)
  {
    std::vector<AddressTake> takes;
    for(auto& s : m_sources)
    {
      if(s->port)
        continue;
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
    if(!takes.empty())
    {
      if(!macro)
        macro.emplace(new PlaceTakes, m_ctx);
      m_placedBox = placeAddressTakes(
          *macro, *m_scenario, m_point.y, takes, m_placement, m_curves);
      placed_any = placed_any || m_placedBox;
    }
  }

  if(macro && placed_any)
    macro->commit();
}
}
