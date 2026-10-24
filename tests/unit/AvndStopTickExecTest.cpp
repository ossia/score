// An interval ends inside a buffer: its process is stopped, then its last,
// partial tick runs, then one more empty tick. Notes a MIDI object starts in
// that last tick must still get their note-off in the empty one.

#include <Process/ExecutionSetup.hpp>

#include <Scenario/Document/Interval/IntervalExecution.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <Execution/BaseScenarioComponent.hpp>
#include <Execution/DocumentPlugin.hpp>

#include <core/document/Document.hpp>

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/graph/tick_methods.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/detail/thread.hpp>
#include <ossia/editor/scenario/scenario.hpp>
#include <ossia/editor/scenario/time_interval.hpp>

#include <QApplication>

#include <Avnd/Factories.hpp>
#include <Crousti/Executor.hpp>
#include <Fx/Arpeggiator_v2.hpp>
#include <Fx/Chord.hpp>
#include <Fx/Quantifier_v2.hpp>
#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>
#include <libremidi/detail/conversion.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Execution.hpp>
#include <score_test/Project.hpp>

#include <map>

namespace
{
using Chord = Nodes::Chord::Node;
const QString chord_uuid = QStringLiteral("F0904279-EA26-48DB-B0DF-F68FE3091DA1");

//! The notes on a MIDI output, by pitch.
struct Notes
{
  std::map<int, int> sounding;
  int note_ons{};

  //! Counts the messages of `out` from index `from` on; returns how many it has.
  std::size_t count(const ossia::midi_port& out, std::size_t from = 0)
  {
    for(std::size_t i = from; i < out.messages.size(); i++)
    {
      const auto m = libremidi::midi1_from_ump(out.messages[i]);
      if(m.size() < 3)
        continue;
      const auto t = m.get_message_type();
      if(t == libremidi::message_type::NOTE_ON && m.bytes[2] > 0)
      {
        sounding[m.bytes[1]]++;
        note_ons++;
      }
      else if(
          t == libremidi::message_type::NOTE_OFF
          || (t == libremidi::message_type::NOTE_ON && m.bytes[2] == 0))
      {
        sounding[m.bytes[1]]--;
      }
    }
    return out.messages.size();
  }

  void take(ossia::midi_port& out)
  {
    count(out);
    out.messages.clear();
  }

  int still_sounding() const
  {
    int n = 0;
    for(auto [p, c] : sounding)
      n += c;
    return n;
  }

  //! Each pitch got as many note-offs as note-ons: an extra note-off of one
  //! pitch does not hide a stuck note of another.
  void check_balanced() const
  {
    for(auto [p, c] : sounding)
    {
      INFO("pitch " << p);
      CHECK(c == 0);
    }
  }
};

template <typename T>
struct Run : Notes
{
  oscr::safe_node<T>& node;
  Execution::DocumentPlugin& plug;
  int64_t date{};
  //! Output messages of the current buffer already counted
  std::size_t seen{};

  Run(oscr::safe_node<T>& n, Execution::DocumentPlugin& p)
      : node{n}
      , plug{p}
  {
  }

  //! A tick of `frames` frames (0: the empty tick after a stop), with a
  //! note-on of `pitch` at its start if any.
  void tick(int frames, int pitch = -1)
  {
    const double rate = plug.context().execState->sampleRate;
    const auto to_flicks = [&](int64_t samples) {
      return ossia::time_value{int64_t(samples * ossia::flicks_per_second<double> / rate)};
    };
    ossia::token_request tk;
    tk.prev_date = to_flicks(date);
    tk.date = to_flicks(date + frames);
    tk.start_sample = 0;
    tk.length_sample = frames;
    date += frames;

    auto& in = *node.root_inputs()[0]->template target<ossia::midi_port>();
    if(pitch >= 0)
      in.messages.push_back(
          libremidi::ump_from_midi1(libremidi::channel_events::note_on(1, pitch, 100)));

    ossia::set_thread_pinned(ossia::thread_type::Audio, 0);
    // A tick with frames is a new buffer, a graph execution of its own; the
    // empty one after a stop shares the buffer of the partial tick before it,
    // and the output port holds what both sent.
    if(frames > 0)
    {
      node.begin_execution();
      seen = 0;
    }
    node.run(tk, ossia::exec_state_facade{plug.context().execState.get()});
    // As the graph does after each token: the node's position moves on
    node.process_time(tk, *plug.context().execState);
    ossia::set_thread_pinned(ossia::thread_type::Ui, 0);
    in.messages.clear();

    seen = count(*node.root_outputs()[0]->template target<ossia::midi_port>(), seen);
  }
};
}

//! Runs `f(run)` on the exec node of a fresh process of type T.
template <typename T, typename F>
void with_process(const QString& uuid, F&& f)
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* proc = score::test::add_process(*doc, uuid, {});
    REQUIRE(proc);

    auto& plug = doc->context().plugin<Execution::DocumentPlugin>();
    plug.reload(true, score::test::base_interval(*doc));
    score::test::run_exec(plug);
    REQUIRE(plug.baseScenario());
    auto& procs = plug.baseScenario()->baseInterval().processes();
    auto it = procs.find(proc->id());
    REQUIRE(it != procs.end());
    auto comp = it->second;
    REQUIRE(comp->node);

    Run<T> r{static_cast<oscr::safe_node<T>&>(*comp->node), plug};
    f(r, *comp);
    QApplication::processEvents();
  });
}

TEST_CASE(
    "A note started in the last tick of a stopped process ends in the extra tick",
    "[avnd][exec][stop]")
{
  with_process<Chord>(chord_uuid, [](auto& r, auto& comp) {
    r.tick(64);
    r.tick(64, 48); // a chord held across the end
    REQUIRE(r.still_sounding() == 3);

    // The interval ends 20 frames into the next buffer: the process is
    // stopped, then that partial tick runs, starting another chord, then the
    // extra empty tick.
    comp.OSSIAProcess().stop();
    r.tick(20, 60);
    CHECK(r.note_ons == 6);
    r.tick(0);
    r.check_balanced();
  });
}

TEMPLATE_TEST_CASE(
    "Notes held when a process stops end in the extra tick", "[avnd][exec][stop]",
    Nodes::Quantifier::v2::Node, Nodes::Arpeggiator::v2::Node)
{
  const QString uuid = std::is_same_v<TestType, Nodes::Quantifier::v2::Node>
                           ? QStringLiteral("a0cb1144-05a8-4f9b-b22c-81cd964314fb")
                           : QStringLiteral("4df48c1c-f5f2-4ab1-9f37-3b0adba139ae");
  with_process<TestType>(uuid, [](auto& r, auto& comp) {
    r.tick(512, 60);
    for(int i = 0; i < 40; i++)
      r.tick(512);
    // Something must be held for the stop to have anything to end
    REQUIRE(r.note_ons > 0);
    REQUIRE(r.still_sounding() > 0);
    comp.OSSIAProcess().stop();
    r.tick(100, 64);
    r.tick(0);
    r.check_balanced();
  });
}

TEMPLATE_TEST_CASE(
    "Notes held when the engine ends an interval inside a buffer end with it",
    "[avnd][exec][stop]", Chord, Nodes::Quantifier::v2::Node,
    Nodes::Arpeggiator::v2::Node)
{
  const QString uuid = std::is_same_v<TestType, Chord> ? chord_uuid
                       : std::is_same_v<TestType, Nodes::Quantifier::v2::Node>
                           ? QStringLiteral("a0cb1144-05a8-4f9b-b22c-81cd964314fb")
                           : QStringLiteral("4df48c1c-f5f2-4ab1-9f37-3b0adba139ae");
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* proc = score::test::add_process(*doc, uuid, {});
    REQUIRE(proc);

    auto& plug = doc->context().plugin<Execution::DocumentPlugin>();
    plug.reload(true, score::test::base_interval(*doc));
    score::test::run_exec(plug);
    REQUIRE(plug.baseScenario());
    auto& base = *plug.baseScenario();
    auto& procs = base.baseInterval().processes();
    auto it = procs.find(proc->id());
    REQUIRE(it != procs.end());
    auto& node = static_cast<oscr::safe_node<TestType>&>(*it->second->node);
    auto& in = *node.root_inputs()[0]->template target<ossia::midi_port>();
    auto& out = *node.root_outputs()[0]->template target<ossia::midi_port>();

    auto& ectx = plug.context();
    auto& st = *ectx.execState;
    auto& itv = *base.baseInterval().OSSIAInterval();
    constexpr int buffer = 512;
    constexpr int last = 40;

    // The interval ends 100 frames into buffer `last`: the scenario stops it in
    // that buffer's temporal tick, after its process was asked for the partial
    // tick, and the stop asks for the extra empty one; the graph then runs both.
    ossia::set_thread_pinned(ossia::thread_type::Audio, 0);
    const ossia::time_value end{
        int64_t(std::llround((last * buffer + 100) * st.samplesToModelRatio))};
    itv.set_min_duration(end);
    itv.set_nominal_duration(end);
    itv.set_max_duration(end);
    itv.start_and_tick();
    ossia::buffer_tick tick{st, *ectx.execGraph, base.baseScenario(), {}};

    Notes notes;
    int sounding_before_last = 0;
    for(int i = 0; i <= last + 4; i++)
    {
      if(i == 0)
        in.messages.push_back(
            libremidi::ump_from_midi1(libremidi::channel_events::note_on(1, 60, 100)));
      if(i == last)
      {
        sounding_before_last = notes.still_sounding();
        // Started in the partial tick: the Chord plays it at once
        in.messages.push_back(
            libremidi::ump_from_midi1(libremidi::channel_events::note_on(1, 64, 100)));
      }
      tick(buffer, double(i * buffer) / st.sampleRate);
      notes.take(out);
      in.messages.clear();
    }
    const bool still_running = itv.running();
    ossia::set_thread_pinned(ossia::thread_type::Ui, 0);

    REQUIRE(!still_running);
    REQUIRE(notes.note_ons > 0);
    CHECK(sounding_before_last > 0);
    notes.check_balanced();
    if constexpr(std::is_same_v<TestType, Chord>)
      CHECK(notes.sounding.contains(64));

    plug.clear();
    QApplication::processEvents();
  });
}
