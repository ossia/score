// A buffer the engine runs a JSFX node in may be cut in several ticks, at a
// loop point or where the process starts: each tick computes its part of the
// buffer and takes the MIDI of its part of the buffer.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Execution.hpp>
#include <score_test/Project.hpp>

#include <Process/Execution/ProcessComponent.hpp>

#include <Scenario/Document/Interval/IntervalExecution.hpp>

#include <Execution/BaseScenarioComponent.hpp>
#include <Execution/DocumentPlugin.hpp>

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/port.hpp>

#include <QTemporaryDir>

#include <libremidi/detail/conversion.hpp>

#include <YSFX/ProcessModel.hpp>
#include <catch2/catch_test_macros.hpp>

namespace
{
const QString ysfx_uuid = QStringLiteral("cb6b50d3-82c0-4356-a282-317da8b8022f");

constexpr int frames = 512;

//! A document with this script as its one JSFX process, playing
struct played_script
{
  QTemporaryDir dir;
  score::Document* doc{};
  YSFX::ProcessModel* proc{};
  Execution::DocumentPlugin* plug{};
  ossia::graph_node* node{};
  ossia::audio_port* audio_out{};
  ossia::midi_port* midi_in{};

  bool open(const score::GUIApplicationContext& ctx, const char* script)
  {
    REQUIRE(dir.isValid());
    const QString path = dir.filePath("ticks.jsfx");
    score::test::write_file(path, script);
    doc = score::test::new_document(ctx);
    REQUIRE(doc);
    proc = static_cast<YSFX::ProcessModel*>(
        score::test::add_process(*doc, ysfx_uuid, path));
    if(!proc || !proc->fx)
      return false;

    plug = &doc->context().plugin<Execution::DocumentPlugin>();
    plug->reload(true, score::test::base_interval(*doc));
    score::test::run_exec(*plug);
    REQUIRE(plug->baseScenario());
    auto& procs = plug->baseScenario()->baseInterval().processes();
    auto it = procs.find(proc->id());
    REQUIRE(it != procs.end());
    REQUIRE(it->second->node);
    node = it->second->node.get();
    for(auto* o : node->root_outputs())
      if(auto* a = o->target<ossia::audio_port>())
        audio_out = a;
    for(auto* i : node->root_inputs())
      if(auto* m = i->target<ossia::midi_port>())
        midi_in = m;
    plug->contextData()->execState->bufferSize = frames;
    return true;
  }

  //! One buffer, as the graph runs it: outputs cleared, then the ticks
  void buffer(std::initializer_list<std::pair<int, int>> ticks)
  {
    auto& state = *plug->contextData()->execState;
    if(audio_out)
      audio_out->set_channels(0);
    for(auto [start, length] : ticks)
    {
      ossia::token_request tk;
      tk.prev_date = ossia::time_value{start * 1000};
      tk.date = ossia::time_value{(start + length) * 1000};
      tk.start_sample = start;
      tk.length_sample = length;
      node->run(tk, ossia::exec_state_facade{&state});
    }
    if(midi_in)
      midi_in->messages.clear();
    state.samples_since_start += frames;
  }

  ~played_script()
  {
    if(plug)
      plug->clear();
  }
};
}

TEST_CASE(
    "a JSFX node keeps what an earlier tick of the buffer wrote",
    "[integration][ysfx]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    played_script p;
    if(!p.open(ctx, R"(desc:ones
in_pin:none
out_pin:out
@sample
spl0 = 1;
)"))
      SKIP("YSFX not built");
    REQUIRE(p.audio_out);

    p.buffer({{0, 200}, {200, frames - 200}});
    REQUIRE(p.audio_out->channels() == 1);
    auto& out = p.audio_out->channel(0);
    REQUIRE(out.size() == std::size_t(frames));
    int ones = 0;
    for(double v : out)
      ones += v == 1.;
    CHECK(ones == frames);
  });
}

TEST_CASE(
    "a JSFX node takes each event of a buffer cut in ticks once",
    "[integration][ysfx]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    played_script p;
    if(!p.open(ctx, R"(desc:note counter
in_pin:none
out_pin:out
@init
notes = 0;
@block
while(midirecv(ofs, msg1, msg2)) (
  (msg1 & 0xF0) == 0x90 ? (
    notes += 1;
    last_offset = ofs;
  );
);
@sample
spl0 = 0;
)"))
      SKIP("YSFX not built");
    REQUIRE(p.midi_in);
    const auto note_at = [&](int64_t frame) {
      const uint64_t w = cmidi2_ump_midi2_note_on(0, 0, 60, 0, 100 << 9, 0);
      libremidi::ump u{uint32_t(w >> 32), uint32_t(w & 0xffffffff)};
      u.timestamp = frame;
      p.midi_in->messages.push_back(u);
    };
    const auto var = [&](const char* name) { return ysfx_read_var(p.proc->fx.get(), name); };

    // A loop point at 200: the note at 300 is the second tick's, 100 in
    note_at(300);
    p.buffer({{0, 200}, {200, frames - 200}});
    CHECK(var("notes") == 1.);
    CHECK(var("last_offset") == 100.);

    // The process starts at 200: a note before it goes at its start
    note_at(50);
    p.buffer({{200, frames - 200}});
    CHECK(var("notes") == 2.);
    CHECK(var("last_offset") == 0.);

    // One tick for the whole buffer
    note_at(10);
    p.buffer({{0, frames}});
    CHECK(var("notes") == 3.);
    CHECK(var("last_offset") == 10.);
  });
}
