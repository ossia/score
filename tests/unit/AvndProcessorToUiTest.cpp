// What a processor sends to the UI from the audio thread: messages to its
// layer (oscr::message_ring and the edition queue) and values of its own
// controls (update_controller, oscr::latest_value). Nothing may allocate on
// the sending side; when the preallocated room is full, messages are dropped.
//
// The other way, what the UI sends to a processor (a control set in the UI, a
// message from its view) is made on the UI thread: the audio thread neither
// allocates nor frees, and what it is done with is freed on the main thread,
// through the GC queue.

#include <Process/Dataflow/WidgetInlets.hpp>
#include <Process/ExecutionContext.hpp>
#include <Process/ExecutionSetup.hpp>

#include <Scenario/Document/Interval/IntervalExecution.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <Execution/BaseScenarioComponent.hpp>
#include <Execution/DocumentPlugin.hpp>

#include <score/document/DocumentInterface.hpp>

#include <core/document/Document.hpp>

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/detail/thread.hpp>

#include <QApplication>
#include <QFileInfo>
#include <QThread>

#include <Avnd/Factories.hpp>
#include <Crousti/Executor.hpp>
#include <Crousti/MessageBus.hpp>
#include <Crousti/ProcessModel.hpp>
#include <AvndProcesses/MIDISync.hpp>
#include <Fx/Chord.hpp>
#include <Fx/Looper/AudioLooper.hpp>
#include <Fx/Looper/MidiLooper.hpp>
#include <Fx/Looper/ValueLooper.hpp>
#include <Fx/RateLimiter.hpp>
#include <Fx/RateLimiter_v2.hpp>
#include <boost/container/static_vector.hpp>

#include <catch2/catch_template_test_macros.hpp>
#include "AllocCounter.hpp"

#include <catch2/catch_test_macros.hpp>
#include <examples/Advanced/MidiScaler/MidiLife.hpp>
#include <libremidi/detail/conversion.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Execution.hpp>
#include <score_test/Project.hpp>

#include <array>
#include <atomic>
#include <cstdlib>
#include <memory>
#include <new>
#include <string>
#include <thread>
#include <set>
#include <vector>

TEST_CASE("Edition queue: try_enqueue does not allocate, and fails when full", "[avnd][message_bus]")
{
  ossia::set_thread_pinned(ossia::thread_type::Ui, 0);
  Execution::EditionCommandQueue q{64};
  int ran = 0;

  int accepted = 0, allocations = -1, accepted_again = 0, allocations_again = -1;
  std::thread audio{[&] {
    ossia::set_thread_pinned(ossia::thread_type::Audio, 0);
    // The first command of a thread registers it as a producer of the queue.
    q.try_enqueue([&ran] { ran++; });
    {
      alloc_counter::scope c;
      while(accepted < 100000 && q.try_enqueue([&ran] { ran++; }))
        accepted++;
      allocations = c.allocations();
    }
  }};
  audio.join();
  CHECK(allocations == 0);
  CHECK(accepted > 0);
  CHECK(accepted < 100000);

  Execution::ExecutionCommand cmd;
  while(q.try_dequeue(cmd))
    cmd();
  CHECK(ran == accepted + 1);

  // The blocks of the commands that ran are free again.
  std::thread audio2{[&] {
    ossia::set_thread_pinned(ossia::thread_type::Audio, 0);
    alloc_counter::scope c;
    for(int i = 0; i < 32 && q.try_enqueue([&ran] { ran++; }); i++)
      accepted_again++;
    allocations_again = c.allocations();
  }};
  audio2.join();
  CHECK(allocations_again == 0);
  CHECK(accepted_again == 32);
  while(q.try_dequeue(cmd))
    cmd();
}

TEST_CASE("Message ring: fixed room, order, wake-ups", "[avnd][message_bus]")
{
  struct message
  {
    std::vector<int> values;
    int id{};
  };
  oscr::message_ring<message, 4> ring;

  // The messages are made before, as an object makes its own.
  std::vector<message> msgs;
  for(int i = 0; i < 6; i++)
    msgs.push_back(message{std::vector<int>(100, i), i});

  int pushed = 0;
  int wakeups = 0;
  {
    alloc_counter::scope c;
    for(auto& m : msgs)
    {
      if(ring.push(std::move(m)))
      {
        pushed++;
        if(ring.request())
          wakeups++;
      }
    }
    CHECK(c.allocations() == 0);
  }
  CHECK(pushed == 4);
  CHECK(ring.dropped() == 2);
  // One wake-up for the reader until it reads.
  CHECK(wakeups == 1);

  std::vector<int> ids;
  ring.drain([&](message&& m) {
    CHECK(m.values.size() == 100);
    ids.push_back(m.id);
  });
  CHECK(ids == std::vector<int>{0, 1, 2, 3});

  // After a read, the next message wakes the reader up again.
  CHECK(ring.push(message{{1, 2, 3}, 10}));
  CHECK(ring.request());
  CHECK_FALSE(ring.request());
  // A wake-up that could not be sent: the next write asks again.
  ring.cancel();
  CHECK(ring.push(message{{}, 11}));
  CHECK(ring.request());
  ids.clear();
  ring.drain([&](message&& m) { ids.push_back(m.id); });
  CHECK(ids == std::vector<int>{10, 11});
  CHECK(ring.drain([](message&&) {}) == 0);
}

TEST_CASE("Latest value: the newest text, without allocation", "[avnd][message_bus]")
{
  oscr::latest_value<std::string> box;
  box.for_each_slot([](std::string& s) { s.reserve(256); });

  CHECK(box.take() == nullptr);
  const std::string a(200, 'a'), b(150, 'b'), c(180, 'c');
  {
    alloc_counter::scope cnt;
    box.back().assign(a);
    box.publish();
    box.back().assign(b);
    box.publish();
    CHECK(cnt.allocations() == 0);
  }
  auto* v = box.take();
  REQUIRE(v);
  CHECK(*v == b);
  CHECK(box.take() == nullptr);
  {
    alloc_counter::scope cnt;
    for(int i = 0; i < 100; i++)
    {
      box.back().assign(i % 2 ? a : c);
      box.publish();
    }
    CHECK(cnt.allocations() == 0);
  }
  v = box.take();
  REQUIRE(v);
  CHECK(*v == a);
}

namespace
{
//! A processor whose view sends it messages that own memory.
struct heavy_receiver
{
  struct message
  {
    std::string text;
    std::vector<float> values;
  };

  // Written without allocating: what process_message saw, in order.
  std::array<int, 8> sizes{};
  std::array<char, 8> first_chars{};
  std::array<float, 8> first_values{};
  int count{};

  void process_message(const message& m)
  {
    if(count >= int(sizes.size()))
      return;
    sizes[count] = int(m.values.size());
    first_chars[count] = m.text.empty() ? 0 : m.text[0];
    first_values[count] = m.values.empty() ? 0.f : m.values[0];
    count++;
  }
};

struct audio_thread_counts
{
  int allocations = -1;
  int frees = -1;
};

//! Runs f as the audio thread, counting what it allocates and frees.
template <typename F>
audio_thread_counts on_audio_thread(F&& f)
{
  audio_thread_counts r;
  ossia::set_thread_pinned(ossia::thread_type::Audio, 0);
  {
    alloc_counter::scope c;
    f();
    r.allocations = c.allocations();
    r.frees = c.frees();
  }
  ossia::set_thread_pinned(ossia::thread_type::Ui, 0);
  return r;
}
}

TEST_CASE("UI to processor bus: messages that own memory are made and freed on the UI thread", "[avnd][message_bus]")
{
  // Messages that are not trivial are serialized, which needs the application.
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    ossia::set_thread_pinned(ossia::thread_type::Ui, 0);
    auto exq = std::make_shared<Execution::ExecutionCommandQueue>(64);
    auto gcq = std::make_shared<Execution::GCCommandQueue>(64);
    heavy_receiver recv;
    std::function<void(QByteArray)> bus = oscr::ui_to_processor_bus(
        recv, std::weak_ptr{exq}, std::weak_ptr{gcq});

    auto send = [&](char c, int n) {
      oscr::MessageBusSender{bus}(heavy_receiver::message{
          std::string(100, c), std::vector<float>(std::size_t(n), float(n))});
    };
    auto run_audio = [&] {
      Execution::ExecutionCommand cmd;
      while(exq->try_dequeue(cmd))
        cmd();
    };
    auto run_gc = [&] {
      for(;;)
      {
        Execution::GCCommand gc;
        if(!gcq->try_dequeue(gc))
          break;
        gc();
      }
    };

    // The first command of a thread registers it as a producer of the GC queue.
    send('w', 1);
    on_audio_thread(run_audio);
    run_gc();
    REQUIRE(recv.count == 1);

    send('a', 10);
    send('b', 20);
    send('c', 30);
    const auto counts = on_audio_thread(run_audio);
    CHECK(counts.allocations == 0);
    CHECK(counts.frees == 0);

    // In the order they were sent, as sent.
    REQUIRE(recv.count == 4);
    CHECK(recv.sizes[1] == 10);
    CHECK(recv.sizes[2] == 20);
    CHECK(recv.sizes[3] == 30);
    CHECK(recv.first_chars[1] == 'a');
    CHECK(recv.first_chars[3] == 'c');
    CHECK(recv.first_values[2] == 20.f);

    // Each message comes back to be freed here: its text, its values, its block.
    int frees = 0;
    {
      alloc_counter::scope c;
      run_gc();
      frees = c.frees();
    }
    CHECK(frees >= 9);
  });
}

namespace
{
using score::test::run_exec;

template <typename T>
struct exec
{
  Process::ProcessModel& proc;
  Execution::DocumentPlugin& plug;
  std::shared_ptr<Execution::ProcessComponent> comp;
  oscr::safe_node<T>& node;
  int64_t date{};

  T& object() { return node.impl.effect; }

  Process::ControlInlet& control(const char* name)
  {
    for(auto* i : proc.inlets())
      if(i->name() == name)
        if(auto* c = qobject_cast<Process::ControlInlet*>(i))
          return *c;
    FAIL("no control " << name);
    throw;
  }
  int index_of(const char* name)
  {
    for(std::size_t i = 0; i < proc.inlets().size(); i++)
      if(proc.inlets()[i]->name() == name)
        return int(i);
    FAIL("no inlet " << name);
    return -1;
  }

  void gui(const char* name, const ossia::value& v)
  {
    control(name).setValue(v);
    run_exec(plug);
  }

  void midi(std::vector<libremidi::message> in)
  {
    auto* port = node.root_inputs()[0]->template target<ossia::midi_port>();
    REQUIRE(port);
    for(auto& m : in)
    {
      libremidi::ump u = libremidi::ump_from_midi1(m);
      port->messages.push_back(u);
    }
  }

  //! Status bytes of the MIDI written to outlet `o` by the last tick.
  std::vector<int> midi_out(int o = 0)
  {
    std::vector<int> r;
    if(auto* port = node.root_outputs()[o]->template target<ossia::midi_port>())
      for(auto& u : port->messages)
        r.push_back(int((u.data[0] >> 16) & 0xFF));
    return r;
  }

  void tick(int frames = 64)
  {
    const double rate = plug.context().execState->sampleRate;
    const auto to_flicks = [&](int64_t samples) {
      return ossia::time_value{
          int64_t(samples * ossia::flicks_per_second<double> / rate)};
    };
    ossia::token_request tk;
    tk.prev_date = to_flicks(date);
    tk.date = to_flicks(date + frames);
    tk.start_sample = 0;
    tk.length_sample = frames;
    date += frames;

    ossia::set_thread_pinned(ossia::thread_type::Audio, 0);
    // Each tick is a graph execution of its own
    node.begin_execution();
    node.run(tk, ossia::exec_state_facade{plug.context().execState.get()});
    ossia::set_thread_pinned(ossia::thread_type::Ui, 0);

    if(auto* port = node.root_inputs()[0]->template target<ossia::midi_port>())
      port->messages.clear();
  }

  //! What the UI thread does on its timer.
  void ui()
  {
    Execution::ExecutionCommand cmd;
    while(plug.context().editionQueue.try_dequeue(cmd))
      cmd();
  }
};

template <typename T, typename F>
void with(const QString& uuid, F&& f)
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* proc = score::test::add_process(*doc, uuid, {});
    if(!proc)
      SKIP("not built");

    auto& plug = doc->context().plugin<Execution::DocumentPlugin>();
    plug.reload(true, score::test::base_interval(*doc));
    run_exec(plug);
    REQUIRE(plug.baseScenario());
    auto& procs = plug.baseScenario()->baseInterval().processes();
    auto it = procs.find(proc->id());
    REQUIRE(it != procs.end());
    REQUIRE(it->second->node);
    auto comp = it->second;
    exec<T> e{*proc, plug, comp, static_cast<oscr::safe_node<T>&>(*comp->node)};
    f(e);
    QApplication::processEvents();
  });
}

libremidi::message note_on(int pitch)
{
  return libremidi::channel_events::note_on(1, pitch, 100);
}
libremidi::message note_off(int pitch)
{
  return libremidi::channel_events::note_off(1, pitch, 0);
}

//! Plays a chord then the key it is learned on, a tick for each.
template <typename E>
void learn_chord(E& e, std::vector<int> chord, int key)
{
  std::vector<libremidi::message> on, off;
  for(int p : chord)
  {
    on.push_back(note_on(p));
    off.push_back(note_off(p));
  }
  e.midi(on);
  e.tick();
  e.midi(off);
  e.tick();
  e.midi({note_on(key)});
  e.tick();
  e.midi({note_off(key)});
  e.tick();
}
}

TEST_CASE("Chord through the binding: a learned chord reaches the Memory control", "[avnd][message_bus][execution]")
{
  with<Nodes::Chord::Node>(
      QStringLiteral("F0904279-EA26-48DB-B0DF-F68FE3091DA1"), [](auto& e) {
    e.gui("Learn", true);
    REQUIRE(e.object().inputs.learn.value);

    auto learn = [&](std::vector<int> chord, int key) {
      std::vector<libremidi::message> on, off;
      for(int p : chord)
      {
        on.push_back(note_on(p));
        off.push_back(note_off(p));
      }
      e.midi(on);
      e.tick();
      e.midi(off);
      e.tick();
      e.midi({note_on(key)});
      e.tick();
      e.midi({note_off(key)});
      e.tick();
    };

    learn({60, 64, 67}, 48);
    learn({62, 65, 69}, 50);
    // Nothing reaches the model before the UI thread runs.
    CHECK(e.control("Memory").value() == ossia::value{std::string{}});
    e.ui();
    // Only the latest text is applied.
    CHECK(e.control("Memory").value() == ossia::value{std::string{"48: 12 16 19; 50: 12 15 19"}});

    learn({64, 67, 71}, 52);
    e.ui();
    CHECK(
        e.control("Memory").value()
        == ossia::value{std::string{"48: 12 16 19; 50: 12 15 19; 52: 12 15 19"}});
  });
}

TEST_CASE("Chord through the binding: a learned chord reaches the Memory control after the queue was full", "[avnd][message_bus][execution]")
{
  with<Nodes::Chord::Node>(
      QStringLiteral("F0904279-EA26-48DB-B0DF-F68FE3091DA1"), [](auto& e) {
    e.gui("Learn", true);
    REQUIRE(e.object().inputs.learn.value);

    // The UI thread is busy (saving, loading): the edition queue fills up.
    ossia::set_thread_pinned(ossia::thread_type::Audio, 0);
    int filled = 0;
    while(filled < 1000000 && e.plug.context().editionQueue.try_enqueue([] { }))
      filled++;
    ossia::set_thread_pinned(ossia::thread_type::Ui, 0);
    REQUIRE(filled < 1000000);

    // The chord is learned once, and nothing writes the Memory again.
    learn_chord(e, {60, 64, 67}, 48);
    e.ui();
    CHECK(e.control("Memory").value() == ossia::value{std::string{}});

    // The next tick sends the wake-up the full queue refused.
    e.tick();
    e.ui();
    CHECK(e.control("Memory").value() == ossia::value{std::string{"48: 12 16 19"}});
  });
}

TEST_CASE("Midi life through the binding: views that are gone are dropped when another is added", "[avnd][message_bus]")
{
  // A process that never sends anything keeps its listeners as long as it is
  // shown: each rebuild of its views adds one.
  with<mtk::MidiLife>(QStringLiteral("04db97fd-2ccf-4271-a5a6-d66552baf0d7"), [](auto& e) {
    auto& model = static_cast<oscr::ProcessModel<mtk::MidiLife>&>(e.proc);
    const auto before = model.to_ui_listeners.size();
    for(int i = 0; i < 10; i++)
    {
      auto view = new QObject;
      model.add_to_ui_listener(view, [](QByteArray) { });
      delete view;
    }
    QObject live;
    model.add_to_ui_listener(&live, [](QByteArray) { });
    CHECK(model.to_ui_listeners.size() <= before + 1);
    for(auto& l : model.to_ui_listeners)
      CHECK(!l.first.isNull());
  });
}

TEST_CASE("Values to fixed-capacity lists keep their first elements", "[avnd][from_value]")
{
  boost::container::static_vector<float, 2> v;
  CHECK(oscr::from_ossia_value_impl{}(ossia::value{ossia::vec4f{1.f, 2.f, 3.f, 4.f}}, v));
  CHECK(v.size() == 2);
  CHECK(v[0] == 1.f);
  CHECK(v[1] == 2.f);
  CHECK(oscr::from_ossia_value_impl{}(ossia::value{ossia::vec3f{5.f, 6.f, 7.f}}, v));
  CHECK(v.size() == 2);
  CHECK(v[1] == 6.f);

  boost::container::static_vector<float, 8> w;
  CHECK(oscr::from_ossia_value_impl{}(ossia::value{ossia::vec2f{1.f, 2.f}}, w));
  CHECK(w.size() == 2);
}

TEST_CASE("Midi life through the binding: the view reaches the layer", "[avnd][message_bus][execution]")
{
  with<mtk::MidiLife>(QStringLiteral("04db97fd-2ccf-4271-a5a6-d66552baf0d7"), [](auto& e) {
    auto& model = static_cast<oscr::ProcessModel<mtk::MidiLife>&>(e.proc);
    int views = 0;
    int last_width = -1;
    model.to_ui = [&](QByteArray b) {
      mtk::life::view_message m;
      oscr::MessageBusReader{b}(m);
      views++;
      last_width = m.width;
    };

    // The view fits in an execution command: it goes through the edition queue.
    static_assert(
        sizeof(QPointer<QObject>) + sizeof(mtk::life::view_message)
        < Execution::ExecutionCommand::max_storage);
    for(int i = 0; i < 200; i++)
      e.tick(256);
    e.ui();
    CHECK(views > 0);
    CHECK(last_width > 0);

    const int before = views;
    for(int i = 0; i < 40; i++)
      e.tick(256);
    e.ui();
    CHECK(views > before);
  });
}

TEST_CASE("Midi life through the binding: every view of the process gets the view",
          "[avnd][message_bus][execution]")
{
  // The process box, the node of the nodal view and the inspector each show
  // the process; a view that is gone stops getting them.
  with<mtk::MidiLife>(QStringLiteral("04db97fd-2ccf-4271-a5a6-d66552baf0d7"), [](auto& e) {
    auto& model = static_cast<oscr::ProcessModel<mtk::MidiLife>&>(e.proc);
    int first = 0, second = 0;
    QObject first_view, *second_view = new QObject;
    model.to_ui_listeners.emplace_back(&first_view, [&](QByteArray) { first++; });
    model.to_ui_listeners.emplace_back(second_view, [&](QByteArray) { second++; });

    for(int i = 0; i < 200; i++)
      e.tick(256);
    e.ui();
    CHECK(first > 0);
    CHECK(second == first);

    delete second_view;
    const int before = second;
    for(int i = 0; i < 40; i++)
      e.tick(256);
    e.ui();
    CHECK(first > before);
    CHECK(second == before);
    CHECK(model.to_ui_listeners.size() == 1);
  });
}

TEMPLATE_TEST_CASE(
    "Loopers through the binding: the view reaches the layer",
    "[avnd][message_bus][execution][looper]", Nodes::AudioLooper::Node,
    Nodes::MidiLooper::Node, Nodes::ValueLooper::Node)
{
  const QString uuid = std::is_same_v<TestType, Nodes::AudioLooper::Node>
                           ? QStringLiteral("a0ad4227-ac3d-448b-a19b-19581ed4e2c6")
                       : std::is_same_v<TestType, Nodes::MidiLooper::Node>
                           ? QStringLiteral("0c0b7d3e-5a52-4b8f-9d1f-6e3a2b8c4f71")
                           : QStringLiteral("399796c5-9e86-4a00-8671-3e759ad6fb33");
  with<TestType>(uuid, [](auto& e) {
    auto& model = static_cast<oscr::ProcessModel<TestType>&>(e.proc);
    int views = 0;
    int mode = -1;
    model.to_ui = [&](QByteArray b) {
      Nodes::Looper::view_message m;
      oscr::MessageBusReader{b}(m);
      views++;
      mode = m.mode;
    };

    e.gui("Mode", ossia::value{std::string{"Record"}});
    for(int i = 0; i < 100; i++)
      e.tick(512);
    e.ui();
    CHECK(views > 0);
    CHECK(mode == int(Nodes::Looper::LoopMode::Record));
  });
}

TEST_CASE("MIDI Sync Out through the binding: Stop goes out once, in the tick after stop", "[avnd][midi_sync_out][execution]")
{
  with<mtk::MIDISyncOut>(QStringLiteral("aa7c1ae5-495e-436e-a079-e3f1a19861bb"), [](auto& e) {
    e.object().inputs.clock.value = mtk::MidiClockMode::Enabled;
    e.object().inputs.clock_startstop.value = mtk::MidiStartStopMode::Enabled;
    int starts = 0, stops = 0;
    auto count = [&] {
      for(int s : e.midi_out())
      {
        starts += s == 0xFA;
        stops += s == 0xFC;
      }
    };
    for(int i = 0; i < 20; i++)
    {
      e.tick(512);
      count();
    }
    CHECK(starts == 1);
    CHECK(stops == 0);

    e.comp->OSSIAProcess().stop();
    // The executor's extra, empty tick.
    e.tick(0);
    count();
    CHECK(stops == 1);
    e.tick(0);
    count();
    CHECK(stops == 1);
  });
}

TEMPLATE_TEST_CASE(
    "Rate limiters: stopping the process drops the value held back",
    "[avnd][execution][stop]", Nodes::RateLimiter::Node, Nodes::RateLimiter::v2::Node)
{
  const QString uuid = std::is_same_v<TestType, Nodes::RateLimiter::Node>
                           ? QStringLiteral("76cfd504-7c10-4bdb-a1b4-fbe449cc06f0")
                           : QStringLiteral("485d39ed-e495-448c-95df-5f2ea905ef5b");
  with<TestType>(uuid, [](auto& e) {
    e.tick(64);
    // A value held back until the end of the interval: a later start must
    // not send it.
    e.object().pending = ossia::value{1.f};
    e.comp->OSSIAProcess().stop();
    CHECK(!e.object().pending);
  });
}

TEST_CASE("MIDI looper through the binding: passthrough", "[avnd][execution][looper]")
{
  with<Nodes::MidiLooper::Node>(QStringLiteral("0c0b7d3e-5a52-4b8f-9d1f-6e3a2b8c4f71"), [](auto& e) {
    INFO("Passthrough port value: " << ossia::value_to_pretty_string(e.control("Passthrough").value()));
    CHECK(e.object().inputs.passthrough.value == Nodes::Looper::Passthrough::Full);
    for(const char* mode : {"Stop", "Play", "Record"})
    {
      e.gui("Mode", ossia::value{std::string{mode}});
      e.tick(64);
      e.midi({note_on(60)});
      e.tick(64);
      INFO(mode);
      CHECK(e.midi_out() == std::vector<int>{0x90});
      e.midi({note_off(60)});
      e.tick(64);
      CHECK(e.midi_out() == std::vector<int>{0x80});
    }
  });
}

TEST_CASE("Value looper through the binding: what is recorded comes back at the outlet",
          "[avnd][execution][looper]")
{
  with<Nodes::ValueLooper::Node>(QStringLiteral("399796c5-9e86-4a00-8671-3e759ad6fb33"), [](auto& e) {
    auto* in = e.node.root_inputs()[0]->template target<ossia::value_port>();
    REQUIRE(in);
    auto out = [&] {
      std::vector<float> r;
      if(auto* port = e.node.root_outputs()[0]->template target<ossia::value_port>())
        for(auto& v : port->get_data())
          if(v.value.get_type() == ossia::val_type::FLOAT)
            r.push_back(v.value.template get<float>());
      return r;
    };
    e.gui("Mode", ossia::value{std::string{"Record"}});
    for(int i = 0; i < 20; i++)
    {
      in->write_value(ossia::value{float(i)}, 10);
      e.tick(512);
      in->clear();
    }
    e.gui("Mode", ossia::value{std::string{"Play"}});
    std::set<float> heard;
    for(int i = 0; i < 40; i++)
    {
      e.tick(512);
      for(float f : out())
        heard.insert(f);
    }
    CHECK(heard.size() >= 18);
    CHECK(heard.count(5.f) == 1);
  });
}

TEST_CASE("Chord through the binding: a text set in the UI is neither allocated nor freed on the audio thread", "[avnd][message_bus][execution][gc]")
{
  with<Nodes::Chord::Node>(
      QStringLiteral("F0904279-EA26-48DB-B0DF-F68FE3091DA1"), [](auto& e) {
    // Longer than a short string: each lives in a heap block.
    const std::string first = "60: 0 4 7; 62: 0 3 7; 64: 0 3 7";
    const std::string second = "48: 12 16 19; 50: 12 15 19; 52: 12 15 19";
    const std::string third = "36: 0 4 7 11; 38: 0 3 7 10; 40: 0 3 7 10";

    // The first command of a thread registers it as a producer of the GC queue.
    e.gui("Memory", ossia::value{first});
    e.plug.contextData()->processEditCommands();
    REQUIRE(e.object().inputs.memory.value == first);

    e.control("Memory").setValue(ossia::value{second});
    e.control("Memory").setValue(ossia::value{third});
    const auto counts = on_audio_thread([&] { e.plug.runAllCommands(); });
    CHECK(counts.allocations == 0);
    CHECK(counts.frees == 0);
    // The latest text wins.
    CHECK(e.object().inputs.memory.value == third);

    // The previous texts are freed when the main thread runs the GC queue.
    int frees = 0;
    {
      alloc_counter::scope c;
      e.plug.contextData()->processEditCommands();
      frees = c.frees();
    }
    CHECK(frees >= 4);
  });
}

TEMPLATE_TEST_CASE(
    "Loopers through the binding: an erase from the view is neither allocated nor freed on the audio thread",
    "[avnd][message_bus][execution][looper][gc]", Nodes::AudioLooper::Node,
    Nodes::MidiLooper::Node)
{
  const QString uuid = std::is_same_v<TestType, Nodes::AudioLooper::Node>
                           ? QStringLiteral("a0ad4227-ac3d-448b-a19b-19581ed4e2c6")
                           : QStringLiteral("0c0b7d3e-5a52-4b8f-9d1f-6e3a2b8c4f71");
  with<TestType>(uuid, [](auto& e) {
    auto& model = static_cast<oscr::ProcessModel<TestType>&>(e.proc);
    // What the layer does when the x of a lane is clicked.
    auto erase = [&] {
      oscr::MessageBusSender{model.from_ui}(
          Nodes::Looper::erase_request{.pass = 0, .layers = 1});
    };
    erase();
    run_exec(e.plug);

    erase();
    erase();
    const auto counts = on_audio_thread([&] { e.plug.runAllCommands(); });
    CHECK(counts.allocations == 0);
    CHECK(counts.frees == 0);
  });
}

TEMPLATE_TEST_CASE(
    "Loopers through the binding: the view drags the last take",
    "[avnd][message_bus][execution][looper][takes]", Nodes::AudioLooper::Node,
    Nodes::MidiLooper::Node)
{
  const QString uuid = std::is_same_v<TestType, Nodes::AudioLooper::Node>
                           ? QStringLiteral("a0ad4227-ac3d-448b-a19b-19581ed4e2c6")
                           : QStringLiteral("0c0b7d3e-5a52-4b8f-9d1f-6e3a2b8c4f71");
  with<TestType>(uuid, [](auto& e) {
    auto& model = static_cast<oscr::ProcessModel<TestType>&>(e.proc);
    Nodes::Looper::looper_view view;
    model.to_ui = [&](QByteArray b) {
      auto m = std::make_unique<Nodes::Looper::view_message>();
      oscr::MessageBusReader{b}(*m);
      view.receive(*m);
    };
    e.gui("Quantize", ossia::value{0.f});
    e.gui("Export", ossia::value{std::string{"Both"}});

    // Plays until the view shows `take` takes.
    auto until_take = [&](int take) {
      for(int i = 0; i < 2000 && view.state.take < take; i++)
      {
        e.tick(256);
        e.ui();
        QThread::msleep(1);
        // The workers' results come back through the UI thread's event loop.
        QApplication::processEvents();
      }
      REQUIRE(view.state.take == take);
    };
    auto dragged = [&] {
      return view.drag_payload(view.width() / 2., view.height() - 10.).files;
    };
    auto pass = [&](const char* mode, int take) {
      e.gui("Mode", ossia::value{std::string{mode}});
      for(int i = 0; i < 100; i++)
        e.tick(256);
      e.gui("Mode", ossia::value{std::string{"Play"}});
      until_take(take);
    };

    pass("Record", 1);
    const auto first = dragged();
    pass("Overdub", 2);
    const auto second = dragged();

    // The mix, then a file for each pass.
    REQUIRE(first.size() == 2);
    REQUIRE(second.size() == 3);
    for(const auto& f : second)
    {
      INFO(f);
      CHECK(QFileInfo::exists(QString::fromStdString(f)));
      CHECK(std::find(first.begin(), first.end(), f) == first.end());
    }
  });
}

TEMPLATE_TEST_CASE(
    "Loopers through the binding: an action shows the mode it asks for in Loop",
    "[avnd][message_bus][execution][looper][switches]", Nodes::AudioLooper::Node,
    Nodes::MidiLooper::Node)
{
  const QString uuid = std::is_same_v<TestType, Nodes::AudioLooper::Node>
                           ? QStringLiteral("a0ad4227-ac3d-448b-a19b-19581ed4e2c6")
                           : QStringLiteral("0c0b7d3e-5a52-4b8f-9d1f-6e3a2b8c4f71");
  with<TestType>(uuid, [](auto& e) {
    // These ticks are not on a musical grid: modes are taken up at once.
    e.gui("Quantize", ossia::value{0.f});
    e.gui("Mode", ossia::value{std::string{"Stop"}});
    e.tick(512);
    auto loop = [&] { return ossia::convert<std::string>(e.control("Mode").value()); };
    auto press = [&](const char* name) {
      e.gui(name, ossia::impulse{});
      for(int i = 0; i < 20; i++)
        e.tick(512);
    };

    press("Rec / Play / Dub");
    CHECK(e.object().transport.actualMode == Nodes::Looper::LoopMode::Record);
    // Nothing reaches the model before the UI thread runs.
    CHECK(loop() == "Stop");
    e.ui();
    CHECK(loop() == "Record");

    press("Rec / Play / Dub");
    e.ui();
    CHECK(loop() == "Play");
    e.gui("Mode", ossia::value{std::string{"Overdub"}});
    e.tick(512);
    CHECK(e.object().transport.actualMode == Nodes::Looper::LoopMode::Overdub);

    // Back to Play by the action: Overdub asked again through Loop is a change.
    press("Rec / Play / Dub");
    e.ui();
    CHECK(loop() == "Play");
    CHECK(e.object().transport.actualMode == Nodes::Looper::LoopMode::Play);
    e.gui("Mode", ossia::value{std::string{"Overdub"}});
    e.tick(512);
    CHECK(e.object().transport.actualMode == Nodes::Looper::LoopMode::Overdub);

    // The UI shows the mode an action asks for without the audio thread
    // allocating, nor the UI's answer when it reaches the node.
    e.gui("Rec / Play / Dub", ossia::impulse{});
    const auto counts = on_audio_thread([&] {
      e.plug.runAllCommands();
      e.tick(512);
    });
    CHECK(counts.allocations == 0);
    e.ui();
    CHECK(loop() == "Play");
    const auto echo = on_audio_thread([&] { e.plug.runAllCommands(); });
    CHECK(echo.allocations == 0);
  });
}

TEMPLATE_TEST_CASE(
    "Loopers through the binding: a mode shown in Loop does not come back to the "
    "looper, which has gone on to another",
    "[avnd][message_bus][execution][looper][switches]", Nodes::AudioLooper::Node,
    Nodes::MidiLooper::Node)
{
  const QString uuid = std::is_same_v<TestType, Nodes::AudioLooper::Node>
                           ? QStringLiteral("a0ad4227-ac3d-448b-a19b-19581ed4e2c6")
                           : QStringLiteral("0c0b7d3e-5a52-4b8f-9d1f-6e3a2b8c4f71");
  with<TestType>(uuid, [](auto& e) {
    using Nodes::Looper::LoopMode;
    e.gui("Quantize", ossia::value{0.f});
    e.gui("Mode", ossia::value{std::string{"Stop"}});
    e.tick(512);
    auto loop = [&] { return ossia::convert<std::string>(e.control("Mode").value()); };

    // Two actions from a footswitch, the UI refreshing between them: Loop
    // shows Record while the looper already plays.
    e.object().inputs.record.value.emplace();
    e.tick(512);
    e.object().inputs.record.value.reset();
    REQUIRE(e.object().transport.actualMode == LoopMode::Record);
    e.ui();
    CHECK(loop() == "Record");
    e.object().inputs.play.value.emplace();
    e.tick(512);
    e.object().inputs.play.value.reset();
    REQUIRE(e.object().transport.actualMode == LoopMode::Play);

    // The UI's commands reach the looper: the later action wins.
    const auto counts = on_audio_thread([&] { e.plug.runAllCommands(); });
    CHECK(counts.allocations == 0);
    for(int i = 0; i < 4; i++)
      e.tick(512);
    CHECK(e.object().transport.actualMode == LoopMode::Play);
    e.ui();
    CHECK(loop() == "Play");
    run_exec(e.plug);
    e.tick(512);
    CHECK(e.object().transport.actualMode == LoopMode::Play);

    // Loop set in the UI is still a change, the same mode it showed included.
    e.gui("Mode", ossia::value{std::string{"Stop"}});
    e.tick(512);
    CHECK(e.object().transport.actualMode == LoopMode::Stop);
  });
}

TEMPLATE_TEST_CASE(
    "Loopers through the binding: a drag or a Send while overdubbing waits for a take "
    "of the loop as it is now",
    "[avnd][message_bus][execution][looper][takes]", Nodes::AudioLooper::Node,
    Nodes::MidiLooper::Node)
{
  const QString uuid = std::is_same_v<TestType, Nodes::AudioLooper::Node>
                           ? QStringLiteral("a0ad4227-ac3d-448b-a19b-19581ed4e2c6")
                           : QStringLiteral("0c0b7d3e-5a52-4b8f-9d1f-6e3a2b8c4f71");
  with<TestType>(uuid, [](auto& e) {
    auto& model = static_cast<oscr::ProcessModel<TestType>&>(e.proc);
    Nodes::Looper::looper_view view;
    model.to_ui = [&](QByteArray b) {
      auto m = std::make_unique<Nodes::Looper::view_message>();
      oscr::MessageBusReader{b}(*m);
      view.receive(*m);
    };
    // What the layer does with the view's requests.
    view.erase = [&](Nodes::Looper::erase_request r) {
      oscr::MessageBusSender{model.from_ui}(r);
    };
    std::vector<halp::timeline_clip> sent;
    view.send_to_timeline = [&](halp::timeline_clip c) { sent.push_back(std::move(c)); };
    e.gui("Quantize", ossia::value{0.f});
    e.gui("Export", ossia::value{std::string{"Mix"}});

    // The view's requests reach the looper through the execution queue.
    auto play = [&](auto until) {
      for(int i = 0; i < 2000 && !until(); i++)
      {
        run_exec(e.plug);
        e.tick(256);
        e.ui();
        QThread::msleep(1);
        QApplication::processEvents();
      }
    };
    auto dragged = [&] {
      return view.drag_payload(view.width() / 2., view.height() - 10.).files;
    };

    e.gui("Mode", ossia::value{std::string{"Record"}});
    for(int i = 0; i < 100; i++)
      e.tick(256);
    e.gui("Mode", ossia::value{std::string{"Overdub"}});
    play([&] { return view.state.take == 1 && view.state.mode == int(Nodes::Looper::LoopMode::Overdub); });
    REQUIRE(view.state.take == 1);
    const auto recording = dragged();
    REQUIRE(recording.size() == 1);

    // A press that would start a drag asks for a take, and the drag waits.
    CHECK(view.mouse_press(view.width() / 2., view.height() - 10.));
    CHECK(view.saving());
    CHECK(dragged().empty());
    play([&] { return !view.saving(); });
    const auto overdub = dragged();
    REQUIRE(overdub.size() == 1);
    CHECK(overdub != recording);
    CHECK(QFileInfo::exists(QString::fromStdString(overdub[0])));
    view.mouse_release(view.width() / 2., view.height() - 10.);
    CHECK(view.state.mode == int(Nodes::Looper::LoopMode::Overdub));

    // Send waits as well, then sends the take it asked for.
    CHECK(view.mouse_press(view.width() - 20., 10.));
    CHECK(sent.empty());
    play([&] { return !sent.empty(); });
    REQUIRE(sent.size() == 1);
    REQUIRE(sent[0].files.size() == 1);
    CHECK(sent[0].files[0] != overdub[0]);
    CHECK(QFileInfo::exists(QString::fromStdString(sent[0].files[0])));
  });
}
