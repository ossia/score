// A CLAP plug-in is activated for at most so many frames per process call. A
// tick longer than that, which another client of the audio server can impose,
// reaches it in several calls of at most that many frames, each with the
// notes dated in it.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

#include <Process/Execution/ProcessComponent.hpp>

#include <Scenario/Document/Interval/IntervalExecution.hpp>

#include <Execution/BaseScenarioComponent.hpp>
#include <Execution/DocumentPlugin.hpp>

#include <core/document/Document.hpp>

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/detail/thread.hpp>

#include <libremidi/message.hpp>
#include <libremidi/ump.hpp>

#include <catch2/catch_test_macros.hpp>

#include <map>

namespace
{
const QString clap_uuid = QStringLiteral("10e8d26d-1d82-4bfc-a9fb-d85ffdf04e5f");

void run_exec(Execution::DocumentPlugin& plug)
{
  ossia::set_thread_pinned(ossia::thread_type::Audio, 0);
  plug.runAllCommands();
  ossia::set_thread_pinned(ossia::thread_type::Ui, 0);
}

libremidi::ump note(int pitch, int64_t frame)
{
  auto u = libremidi::ump_from_midi1(libremidi::channel_events::note_on(1, pitch, 100));
  u.timestamp = frame;
  return u;
}
}

TEST_CASE(
    "CLAP: a tick longer than the activated block size is split, notes dated per block",
    "[integration][clap][buffer_size][midi]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* proc = score::test::add_process(
        *doc, clap_uuid,
        QStringLiteral(CLAP_BLOCK_RECORDER_PATH ":::io.ossia.test.block-recorder"));
    REQUIRE(proc);

    auto& plug = doc->context().plugin<Execution::DocumentPlugin>();
    plug.reload(true, score::test::base_interval(*doc));
    run_exec(plug);
    REQUIRE(plug.baseScenario());
    auto& procs = plug.baseScenario()->baseInterval().processes();
    auto it = procs.find(proc->id());
    REQUIRE(it != procs.end());
    REQUIRE(it->second->node);
    auto& node = *it->second->node;

    ossia::midi_port* midi{};
    for(auto* in : node.root_inputs())
      if(auto* p = in->target<ossia::midi_port>())
        midi = p;
    ossia::audio_port* audio{};
    for(auto* out : node.root_outputs())
      if(auto* p = out->target<ossia::audio_port>())
        audio = p;
    REQUIRE(midi);
    REQUIRE(audio);

    auto& state = *plug.contextData()->execState;
    const int block = state.setupBufferSize();
    REQUIRE(block > 4);
    const int frames = block * 2 + block / 2;
    // In the first block, at the start of the second, inside it, near the
    // end of the last, and dated before and after the tick
    const std::map<int, int64_t> sent{
        {60, 10}, {61, block}, {62, block + 3}, {63, frames - 2}, {64, -5}, {65, frames * 4}};
    for(auto [pitch, frame] : sent)
      midi->messages.push_back(note(pitch, frame));

    const int setup_size = state.bufferSize;
    state.bufferSize = frames;
    ossia::token_request tk;
    tk.prev_date = ossia::time_value{0};
    tk.date = ossia::time_value{int64_t(frames) * 1000};
    tk.start_sample = 0;
    tk.length_sample = frames;
    ossia::set_thread_pinned(ossia::thread_type::Audio, 0);
    node.run(tk, ossia::exec_state_facade{&state});
    ossia::set_thread_pinned(ossia::thread_type::Ui, 0);
    state.bufferSize = setup_size;

    REQUIRE(audio->channels() >= 2);
    const auto& sizes = audio->channel(0);
    const auto& keys = audio->channel(1);
    REQUIRE(sizes.size() >= std::size_t(frames));
    REQUIRE(keys.size() >= std::size_t(frames));

    // Two calls of the block size, then the rest
    for(int i = 0; i < frames; i++)
    {
      INFO("frame " << i);
      REQUIRE(sizes[i] == (i < 2 * block ? block : frames - 2 * block));
    }

    // Each note once, where it was dated; the earlier one at the start, the
    // later one on the last frame
    std::map<int, int> got;
    for(int i = 0; i < frames; i++)
      if(keys[i] != 0.)
        got[int(keys[i])] = i;
    CHECK(got
          == (std::map<int, int>{
              {60, 10},
              {61, block},
              {62, block + 3},
              {63, frames - 2},
              {64, 0},
              {65, frames - 1}}));
  });
}
