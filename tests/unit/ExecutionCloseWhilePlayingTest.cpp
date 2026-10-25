// App test: closing a document while it plays, with processes whose MIDI inlet
// is bound to a MIDI device.
//
// Closing tears the execution plug-in down first: stopping releases the play
// tick, whose teardown queues the unregistration of every graph node on the
// execution queue and waits for the audio thread to run it. The device
// explorer plug-in is closed right after and destroys every device. An
// unregistration that runs after that reads the destroyed device through the
// inlet's address (execution_state::unregister_port).
//
// The audio thread is held for 1.5 s in the first stopped tick, longer than a
// wait counted in spins lasts: the teardown is then still queued when the
// devices close. The device and its protocol live in pages made inaccessible
// once destroyed, so that reading them faults.
//
// One app per case: run the cases separately on Windows.

#include <Device/Protocol/DeviceInterface.hpp>
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Device/Protocol/ProtocolList.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/ExecutionAction.hpp>
#include <Process/Process.hpp>

#include <Scenario/Document/Interval/IntervalExecution.hpp>

#include <Audio/AudioApplicationPlugin.hpp>
#include <Audio/AudioTick.hpp>
#include <Audio/Settings/Model.hpp>
#include <Engine/ApplicationPlugin.hpp>
#include <Execution/BaseScenarioComponent.hpp>
#include <Execution/DocumentPlugin.hpp>
#include <Execution/ExecutionController.hpp>
#include <Explorer/Commands/Add/LoadDevice.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>

#include <core/document/Document.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <ossia/dataflow/graph/graph_interface.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/detail/algorithms.hpp>
#include <ossia/network/base/protocol.hpp>
#include <ossia/network/common/complex_type.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <ossia/protocols/midi/midi_stream.hpp>

#include <QApplication>
#include <QElapsedTimer>

#include <catch2/catch_all.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

#include <atomic>
#include <chrono>
#include <memory>
#include <new>
#include <thread>
#include <vector>

#if __has_include(<sys/mman.h>)
#include <sys/mman.h>
#define CLOSE_WHILE_PLAYING_TEST 1

namespace
{
void spin(int ms)
{
  QElapsedTimer t;
  t.start();
  do
  {
    QApplication::processEvents(QEventLoop::AllEvents, 5);
  } while(t.elapsed() < ms);
}

//! Large enough for a device or a protocol
constexpr std::size_t page_block = 1 << 16;

void* poisonable_alloc()
{
  void* p = mmap(
      nullptr, page_block, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if(p == MAP_FAILED)
    throw std::bad_alloc{};
  return p;
}

//! Kept mapped, never readable again: a use after free faults.
void poison(void* p) noexcept
{
  mprotect(p, page_block, PROT_NONE);
}

//! The graph nodes of the processes under test and their graph, kept alive
//! past the close.
std::vector<std::shared_ptr<ossia::graph_node>> g_nodes;
std::shared_ptr<ossia::graph_interface> g_graph;
//! The played context, as the stopped tick's own GC queue keeps it between
//! two ticks: its commands still run once the audio thread is released.
std::shared_ptr<Execution::DocumentPlugin::ContextData> g_ctx;
//! How many of them were still in the graph when their device was destroyed.
int g_registered_at_teardown = -1;
int g_teardowns = 0;

//! Holds the audio thread in the first stopped tick once armed.
struct StallFirstStoppedTick final : Execution::ExecutionAction
{
  SCORE_CONCRETE("6f4a0c2d-8e51-4b7a-9d3c-2a1e5f7b8c90")
public:
  std::atomic_bool armed{};
  void startTick(const ossia::audio_tick_state&) override
  {
    if(Audio::execution_status.load() == ossia::transport_status::stopped
       && armed.exchange(false))
      std::this_thread::sleep_for(std::chrono::milliseconds{1500});
  }
};

//! A MIDI stream on a plain tree, as the MIDI device descriptions and the
//! control surfaces are: execution_state registers the inlets bound to it.
struct FakeMidiProtocol final
    : ossia::net::protocol_base
    , ossia::net::midi::midi_stream
{
  FakeMidiProtocol()
      : protocol_base{flags{}}
  {
  }

  ~FakeMidiProtocol() override
  {
    // The queued unregistration removes the node from the graph once its
    // ports are unregistered.
    g_teardowns++;
    g_registered_at_teardown = 0;
    if(g_graph)
      for(auto& n : g_nodes)
        if(ossia::contains(g_graph->get_nodes(), n.get()))
          g_registered_at_teardown++;
  }

  static void* operator new(std::size_t n)
  {
    if(n > page_block)
      throw std::bad_alloc{};
    return poisonable_alloc();
  }
  static void operator delete(void* p) noexcept { poison(p); }

  bool pull(ossia::net::parameter_base&) override { return false; }
  bool push(const ossia::net::parameter_base&, const ossia::value&) override
  {
    return false;
  }
  bool push_raw(const ossia::net::full_parameter_data&) override { return false; }
  bool observe(ossia::net::parameter_base&, bool) override { return false; }
  bool update(ossia::net::node_base&) override { return false; }

  libremidi::midi_in* midi_in() const noexcept override { return nullptr; }
  void enable_registration() override { }
  void push_value(const libremidi::ump&) override { }
  void push_value(const libremidi::message&) override { }
};

class FakeMidiDevice final : public Device::OwningDeviceInterface
{
public:
  explicit FakeMidiDevice(const Device::DeviceSettings& s)
      : OwningDeviceInterface{s}
  {
  }

  bool reconnect() override
  {
    disconnect();
    static_assert(sizeof(ossia::net::generic_device) <= page_block);
    void* mem = poisonable_alloc();
    auto dev = std::shared_ptr<ossia::net::generic_device>(
        new(mem) ossia::net::generic_device{
            std::make_unique<FakeMidiProtocol>(), settings().name.toStdString()},
        [mem](ossia::net::generic_device* d) {
      d->~generic_device();
      poison(mem);
    });
    ossia::create_parameter(dev->get_root_node(), "/in", "int");
    m_dev = dev;
    deviceChanged(nullptr, m_dev.get());
    connectionChanged(true);
    return true;
  }

  Device::Node refresh() override { return simple_refresh(); }
};

class FakeMidiSettingsWidget final : public Device::ProtocolSettingsWidget
{
public:
  Device::DeviceSettings m_settings;
  Device::DeviceSettings getSettings() const override { return m_settings; }
  void setSettings(const Device::DeviceSettings& s) override { m_settings = s; }
};

class FakeMidiFactory final : public Device::ProtocolFactory
{
  SCORE_CONCRETE("3c1d6a2e-9b47-4f0e-8d55-0e2f7a6b9c13")
public:
  QString prettyName() const noexcept override { return QStringLiteral("Fake MIDI"); }
  QString category() const noexcept override { return QStringLiteral("Test"); }

  Device::DeviceInterface* makeDevice(
      const Device::DeviceSettings& s, const Explorer::DeviceDocumentPlugin&,
      const score::DocumentContext&) override
  {
    return new FakeMidiDevice{s};
  }
  Device::ProtocolSettingsWidget* makeSettingsWidget() override
  {
    return new FakeMidiSettingsWidget;
  }
  Device::AddressDialog* makeAddAddressDialog(
      const Device::DeviceInterface&, const score::DocumentContext&, QWidget*) override
  {
    return nullptr;
  }
  Device::AddressDialog* makeEditAddressDialog(
      const Device::AddressSettings&, const Device::DeviceInterface&,
      const score::DocumentContext&, QWidget*) override
  {
    return nullptr;
  }
  const Device::DeviceSettings& defaultSettings() const noexcept override
  {
    static const Device::DeviceSettings s = [] {
      Device::DeviceSettings set;
      set.name = QStringLiteral("fmidi");
      set.protocol = static_concreteKey();
      return set;
    }();
    return s;
  }
  void serializeProtocolSpecificSettings(
      const QVariant&, const VisitorVariant&) const override
  {
  }
  QVariant makeProtocolSpecificSettings(const VisitorVariant&) const override
  {
    return {};
  }
  bool checkCompatibility(const Device::DeviceSettings&, const Device::DeviceSettings&)
      const noexcept override
  {
    return true;
  }
};

void registerFakeMidiProtocol(const score::GUIApplicationContext& ctx)
{
  auto& list = const_cast<Device::ProtocolFactoryList&>(
      ctx.interfaces<Device::ProtocolFactoryList>());
  if(!list.get(FakeMidiFactory::static_concreteKey()))
    list.insert(std::make_unique<FakeMidiFactory>());
}

StallFirstStoppedTick& registerStall(const score::GUIApplicationContext& ctx)
{
  auto& list = const_cast<Execution::ExecutionActionList&>(
      ctx.interfaces<Execution::ExecutionActionList>());
  if(!list.get(StallFirstStoppedTick::static_concreteKey()))
    list.insert(std::make_unique<StallFirstStoppedTick>());
  return static_cast<StallFirstStoppedTick&>(
      *list.get(StallFirstStoppedTick::static_concreteKey()));
}

Process::MidiInlet* midiInlet(Process::ProcessModel& p)
{
  for(auto* inlet : p.inlets())
    if(auto* m = qobject_cast<Process::MidiInlet*>(inlet))
      return m;
  return nullptr;
}

const QString midi_looper_uuid = QStringLiteral("0c0b7d3e-5a52-4b8f-9d1f-6e3a2b8c4f71");
const QString chord_uuid = QStringLiteral("F0904279-EA26-48DB-B0DF-F68FE3091DA1");

void closeWhilePlaying(const score::GUIApplicationContext& ctx, QString uuid, int count)
{
  g_nodes.clear();
  g_graph.reset();
  g_ctx.reset();
  g_registered_at_teardown = -1;
  g_teardowns = 0;
  registerFakeMidiProtocol(ctx);
  auto& stall = registerStall(ctx);

  // 8192 frames: sync() in set_tick() waits long enough to see the stopped
  // tick replace the playing one.
  auto& set = ctx.settings<Audio::Settings::Model>();
  // Settings are saved in the tests' shared profile: put them back after.
  struct restore_audio_settings
  {
    Audio::Settings::Model& set;
    int rate = set.getRate();
    int buffer = set.getBufferSize();
    ~restore_audio_settings()
    {
      set.setRate(rate);
      set.setBufferSize(buffer);
      set.changed();
    }
  } restore{set};
  set.setRate(44100);
  set.setBufferSize(8192);
  set.changed();
  spin(50);

  auto doc = score::test::new_document(ctx);
  REQUIRE(doc);
  REQUIRE(ctx.guiApplicationPlugin<Audio::ApplicationPlugin>().audio);

  auto& devices = doc->context().plugin<Explorer::DeviceDocumentPlugin>();
  CommandDispatcher<>{doc->context().commandStack}.submit(
      new Explorer::Command::LoadDevice{
          devices, Device::Node{FakeMidiFactory{}.defaultSettings(), nullptr}});
  REQUIRE(devices.list().findDevice(QStringLiteral("fmidi")));

  std::vector<Id<Process::ProcessModel>> ids;
  for(int i = 0; i < count; i++)
  {
    auto proc = score::test::add_process(*doc, uuid, {});
    if(!proc)
      SKIP("process " << uuid.toStdString() << " is not built");
    auto in = midiInlet(*proc);
    REQUIRE(in);
    in->setAddress(State::AddressAccessor{*State::Address::fromString("fmidi:/in")});
    ids.push_back(proc->id());
  }

  auto& plug = doc->context().plugin<Execution::DocumentPlugin>();
  auto& exec = ctx.guiApplicationPlugin<Engine::ApplicationPlugin>().execution();
  exec.request_play_global(true);
  // A few ticks: the ports are bound from the execution queue
  spin(1000);
  REQUIRE(plug.isPlaying());

  g_graph = plug.contextData()->execGraph;
  g_ctx = plug.contextData();
  REQUIRE(g_graph);
  auto& itv = plug.baseScenario()->baseInterval();
  for(auto& id : ids)
  {
    auto it = itv.processes().find(id);
    REQUIRE(it != itv.processes().end());
    REQUIRE(it->second->node);
    REQUIRE(ossia::contains(g_graph->get_nodes(), it->second->node.get()));
    g_nodes.push_back(it->second->node);
  }

  stall.armed = true;
  ctx.docManager.forceCloseDocument(ctx, *doc);
  // Leftover commands run once the audio thread is released
  spin(2500);

  CHECK(g_teardowns == 1);
  CHECK(g_registered_at_teardown == 0);
  g_nodes.clear();
  g_graph.reset();
  g_ctx.reset();
}
}

#endif

TEST_CASE(
    "Closing a playing document unregisters MIDI loopers before their device goes",
    "[execution][devices][midi]")
{
#if defined(CLOSE_WHILE_PLAYING_TEST)
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    closeWhilePlaying(ctx, midi_looper_uuid, 2);
  });
#else
  SKIP("needs mprotect");
#endif
}

TEST_CASE(
    "Closing a playing document unregisters any MIDI process before its device goes",
    "[execution][devices][midi]")
{
#if defined(CLOSE_WHILE_PLAYING_TEST)
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    closeWhilePlaying(ctx, chord_uuid, 1);
  });
#else
  SKIP("needs mprotect");
#endif
}
