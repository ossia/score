// A MIDI process that goes away while it holds a note -- deleted, unplugged,
// stopped, moved on the timeline, muted, paused, or the score stopping --
// must not leave the note sounding in the process after it, nor on the MIDI
// device it writes to. Likewise for a port pointed at another device while a
// note is held through it. Checked in a document played by the engine.

#include <Process/Dataflow/Port.hpp>
#include <Process/ExecutionSetup.hpp>

#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Commands/Interval/RemoveProcessFromInterval.hpp>
#include <Scenario/Document/Interval/IntervalExecution.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <Dataflow/Commands/EditConnection.hpp>
#include <Execution/BaseScenarioComponent.hpp>
#include <Execution/Clock/DefaultClock.hpp>
#include <Execution/DocumentPlugin.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentInterface.hpp>

#include <core/document/Document.hpp>

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/graph/tick_methods.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/detail/thread.hpp>
#include <ossia/editor/scenario/scenario.hpp>
#include <ossia/editor/scenario/time_interval.hpp>
#include <ossia/network/base/protocol.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <ossia/protocols/midi/midi_stream.hpp>

#include <QApplication>

#include <Avnd/Factories.hpp>
#include <Crousti/Executor.hpp>
#include <Fx/Chord.hpp>
#include <catch2/catch_test_macros.hpp>
#include <libremidi/detail/conversion.hpp>
#include <libremidi/libremidi.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Execution.hpp>
#include <score_test/Process.hpp>
#include <score_test/Project.hpp>

#include <string>
#include <vector>

namespace
{
// Plays what it is given at once, and keeps it until the note-off.
const QString chord_uuid = QStringLiteral("F0904279-EA26-48DB-B0DF-F68FE3091DA1");

//! A MIDI device that remembers what it was sent.
struct fake_midi_protocol final
    : ossia::net::protocol_base
    , ossia::net::midi::midi_stream
{
  std::vector<libremidi::ump> sent;
  //! Set for a device that is also read from
  std::unique_ptr<libremidi::midi_in> input;

  void open_input()
  {
    input = std::make_unique<libremidi::midi_in>(
        libremidi::input_configuration{.on_message = [](const libremidi::message&) {}},
        libremidi::dummy_configuration{});
  }

  bool pull(ossia::net::parameter_base&) override { return false; }
  bool push(const ossia::net::parameter_base&, const ossia::value&) override
  {
    return false;
  }
  bool push_raw(const ossia::net::full_parameter_data&) override { return false; }
  bool observe(ossia::net::parameter_base&, bool) override { return false; }
  bool update(ossia::net::node_base&) override { return false; }

  libremidi::midi_in* midi_in() const noexcept override { return input.get(); }
  void enable_registration() override { }
  void push_value(const libremidi::ump& m) override { sent.push_back(m); }
  void push_value(const libremidi::message&) override { }

  //! Notes the device was told to start and not to stop
  int sounding() const
  {
    ossia::midi_note_tracker t;
    for(auto& m : sent)
      t.track(m);
    return t.count();
  }
};

enum class Action
{
  RemoveProcess,
  RemoveCable,
  IntervalEnds,
  Transport,
  Offset,
  Mute,
  Pause,
  ScoreStops,
  OutletRebound,
  InletRebound
};

const char* name(Action a)
{
  switch(a)
  {
    case Action::RemoveProcess: return "the process is deleted";
    case Action::RemoveCable: return "the cable is deleted";
    case Action::IntervalEnds: return "the interval of the process ends";
    case Action::Transport: return "the score is moved elsewhere (transport)";
    case Action::Offset: return "the score is moved elsewhere (offset)";
    case Action::Mute: return "the interval of the process is muted";
    case Action::Pause: return "the score is paused";
    case Action::ScoreStops: return "the score stops";
    case Action::OutletRebound: return "its output is sent to another device";
    case Action::InletRebound: return "its input is read from another device";
  }
  return "";
}

template <typename T>
T* first_port(const auto& ports)
{
  for(auto* p : ports)
    if(auto* t = qobject_cast<T*>(p))
      return t;
  return nullptr;
}

ossia::midi_outlet& midi_out(ossia::graph_node& n)
{
  for(auto* o : n.root_outputs())
    if(o->which() == ossia::midi_port::which)
      return *static_cast<ossia::midi_outlet*>(o);
  FAIL("no MIDI outlet");
  throw;
}

ossia::inlet& midi_inlet(ossia::graph_node& n)
{
  for(auto* i : n.root_inputs())
    if(i->which() == ossia::midi_port::which)
      return *i;
  FAIL("no MIDI inlet");
  throw;
}

ossia::midi_port& midi_in(ossia::graph_node& n)
{
  for(auto* i : n.root_inputs())
    if(auto* p = i->target<ossia::midi_port>())
      return *p;
  FAIL("no MIDI inlet");
  throw;
}
}

TEST_CASE(
    "A MIDI process that goes away while it holds a note leaves nothing sounding",
    "[midi][exec][release][app]")
{
  for(const Action action :
      {Action::RemoveProcess, Action::RemoveCable, Action::IntervalEnds, Action::Transport,
       Action::Offset, Action::Mute, Action::Pause, Action::ScoreStops,
       Action::OutletRebound, Action::InletRebound})
  {
    DYNAMIC_SECTION("when " << name(action))
    {
      // Outlives the execution, which sends to it until its last command.
      auto proto_ptr = std::make_unique<fake_midi_protocol>();
      auto& proto = *proto_ptr;
      ossia::net::generic_device dev{std::move(proto_ptr), "midi"};
      // The keys are played on this one
      auto keyboard_ptr = std::make_unique<fake_midi_protocol>();
      auto& keyboard = *keyboard_ptr;
      keyboard.open_input();
      ossia::net::generic_device keyboard_dev{std::move(keyboard_ptr), "keyboard"};
      auto other_ptr = std::make_unique<fake_midi_protocol>();
      other_ptr->open_input();
      ossia::net::generic_device other_dev{std::move(other_ptr), "other"};

      score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
        auto* doc = score::test::new_document(ctx);
        REQUIRE(doc);
        auto& scenario = score::test::base_scenario(*doc);

        // An interval of two seconds from the start of the score, with two
        // processes: one cabled to a process that plays for the whole score,
        // the other one reading a MIDI device and sending to another.
        Scenario::IntervalModel* box{};
        Process::ProcessModel* to_cable{};
        Process::ProcessModel* to_device{};
        {
          Scenario::Command::Macro m{
              new Scenario::Command::AddProcessInNewBoxMacro, doc->context()};
          auto& from = m.createState(scenario, scenario.startEvent().id(), 0.6);
          box = &m.createIntervalAfter(
              scenario, from.id(), Scenario::Point{TimeVal::fromMsecs(2000), 0.6});
          const auto key = UuidKey<Process::ProcessModel>::fromString(chord_uuid);
          to_cable = m.createProcessInNewSlot(*box, key, QString{});
          to_device = m.createProcessInNewSlot(*box, key, QString{});
          m.commit();
        }
        REQUIRE(to_cable);
        REQUIRE(to_device);
        auto* sink = score::test::add_process(*doc, chord_uuid, {});
        REQUIRE(sink);

        auto* cable_out = first_port<Process::MidiOutlet>(to_cable->outlets());
        auto* cable_in = first_port<Process::MidiInlet>(sink->inlets());
        REQUIRE(cable_out);
        REQUIRE(cable_in);
        auto& model = score::IDocument::get<Scenario::ScenarioDocumentModel>(*doc);
        CommandDispatcher<> disp{doc->context().commandStack};
        disp.submit<Dataflow::CreateCable>(
            model, Id<Process::Cable>{1}, Process::CableType::ImmediateGlutton, *cable_out,
            *cable_in);
        auto cable = model.cables.find(Id<Process::Cable>{1});
        REQUIRE(cable != model.cables.end());

        auto& plug = doc->context().plugin<Execution::DocumentPlugin>();
        plug.reload(true, score::test::base_interval(*doc));
        score::test::run_exec(plug);
        REQUIRE(plug.baseScenario());
        auto& base = *plug.baseScenario();

        Execution::IntervalComponent* box_comp{};
        for(auto* cmp : box->components())
          if(auto ic = dynamic_cast<Execution::IntervalComponent*>(cmp))
            box_comp = ic;
        REQUIRE(box_comp);
        auto node_of = [](auto& procs, Process::ProcessModel& p) {
          auto it = procs.find(p.id());
          REQUIRE(it != procs.end());
          return it->second->node;
        };
        // Kept alive: the nodes are looked at after their processes are gone.
        const auto cable_node = node_of(box_comp->processes(), *to_cable);
        const auto device_node = node_of(box_comp->processes(), *to_device);
        const auto sink_node = node_of(base.baseInterval().processes(), *sink);

        auto& ectx = plug.context();
        auto& st = *ectx.execState;
        auto& itv = *base.baseInterval().OSSIAInterval();

        ossia::set_thread_pinned(ossia::thread_type::Audio, 0);
        st.register_device(&dev);
        st.register_device(&keyboard_dev);
        st.register_device(&other_dev);
        st.apply_device_changes();
        auto& device_in = midi_inlet(*device_node);
        device_in.address = &keyboard_dev.get_root_node();
        st.register_port(device_in);
        auto& device_out = midi_out(*device_node);
        device_out.address = &dev.get_root_node();
        st.register_port(device_out);

        const ossia::time_value end{int64_t(1e5 * ossia::flicks_per_second<double>)};
        itv.set_min_duration(end);
        itv.set_nominal_duration(end);
        itv.set_max_duration(end);
        itv.start_and_tick();
        ossia::buffer_tick tick{st, *ectx.execGraph, base.baseScenario(), {}};

        const int buffer = st.bufferSize;
        int64_t frames = 0;
        auto run = [&](double seconds) {
          const int64_t n = int64_t(seconds * st.sampleRate);
          for(int64_t i = 0; i < n; i += buffer)
          {
            tick(buffer, double(frames) / st.sampleRate);
            frames += buffer;
          }
        };
        // What the sink passes on is what it was told to play.
        auto sink_sounding = [&] { return midi_out(*sink_node).held.count(); };

        // Half a second in, a key goes down on both processes.
        run(0.5);
        const auto on
            = libremidi::ump_from_midi1(libremidi::channel_events::note_on(1, 60, 100));
        midi_in(*cable_node).messages.push_back(on);
        keyboard.receive_ump(on);
        run(0.25);
        // A chord of a chord: several notes for the key.
        REQUIRE(sink_sounding() > 0);
        REQUIRE(proto.sounding() > 0);

        auto as_ui = [&](auto f) {
          ossia::set_thread_pinned(ossia::thread_type::Ui, 0);
          f();
          QApplication::processEvents();
          score::test::run_exec(plug);
          ossia::set_thread_pinned(ossia::thread_type::Audio, 0);
        };

        bool has_sink = true;
        switch(action)
        {
          case Action::RemoveProcess:
            as_ui([&] {
              disp.submit<Scenario::Command::RemoveProcessFromInterval>(*box, to_cable->id());
              disp.submit<Scenario::Command::RemoveProcessFromInterval>(
                  *box, to_device->id());
            });
            run(0.1);
            break;
          case Action::RemoveCable:
            as_ui([&] { disp.submit<Dataflow::RemoveCable>(model, *cable); });
            run(0.1);
            break;
          case Action::IntervalEnds:
            run(1.5);
            break;
          case Action::Transport:
            itv.transport(
                ossia::time_value{int64_t(1.5 * ossia::flicks_per_second<double>)});
            run(0.1);
            break;
          case Action::Offset:
            itv.offset(ossia::time_value{int64_t(1.5 * ossia::flicks_per_second<double>)});
            st.commit();
            run(0.1);
            break;
          case Action::Mute:
            as_ui([&] { box->setMuted(true); });
            run(0.1);
            break;
          case Action::Pause:
            // The engine stops ticking the score while it is paused, and only
            // runs the commands it is sent.
            as_ui([&] {
              Execution::DefaultClock clock{ectx};
              clock.pause(base);
            });
            has_sink = false;
            break;
          case Action::ScoreStops:
            // As the engine does: the clock stops the score, the audio thread
            // runs what that queued, and the execution is taken down.
            as_ui([&] {
              Execution::DefaultClock clock{ectx};
              clock.stop(base);
            });
            ossia::set_thread_pinned(ossia::thread_type::Ui, 0);
            plug.clear();
            QApplication::processEvents();
            has_sink = false;
            break;
          case Action::OutletRebound:
            as_ui([&] {
              first_port<Process::MidiOutlet>(to_device->outlets())
                  ->setAddress(State::AddressAccessor{State::Address{"other", {}}});
            });
            run(0.1);
            // The cabled process goes on holding its note.
            has_sink = false;
            break;
          case Action::InletRebound:
            // The key is still down on the keyboard: its note off will come
            // when nothing reads it any more.
            as_ui([&] {
              first_port<Process::MidiInlet>(to_device->inlets())
                  ->setAddress(State::AddressAccessor{State::Address{"other", {}}});
            });
            run(0.1);
            has_sink = false;
            break;
        }
        ossia::set_thread_pinned(ossia::thread_type::Ui, 0);

        if(has_sink)
          CHECK(sink_sounding() == 0);
        if(action != Action::RemoveCable)
          CHECK(proto.sounding() == 0);

        if(action != Action::ScoreStops)
        {
          plug.clear();
          QApplication::processEvents();
        }
      });
    }
  }
}
