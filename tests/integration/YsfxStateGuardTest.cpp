// A JSFX script's code never runs on two threads at once: reading or writing
// its state (a preset, an undo, a save) runs its @serialize section, and the
// audio thread runs @sample on the same memory. The audio thread never waits
// for it: it skips the tick, and its events go to the next one.

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

#include <QElapsedTimer>
#include <QFile>
#include <QTemporaryDir>

#include <YSFX/ProcessModel.hpp>
#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <thread>

namespace
{
const QString ysfx_uuid = QStringLiteral("cb6b50d3-82c0-4356-a282-317da8b8022f");

constexpr auto script = R"(desc:guard test
out_pin:out
@init
calls = 0;
kept = 0;
@sample
calls += 1;
spl0 = 1;
@serialize
file_var(0, kept);
)";

YSFX::ProcessModel* addScript(score::Document& doc, const QTemporaryDir& dir)
{
  const QString path = dir.filePath("guard.jsfx");
  QFile f{path};
  if(!f.open(QIODevice::WriteOnly))
    return nullptr;
  f.write(script);
  f.close();
  return static_cast<YSFX::ProcessModel*>(
      score::test::add_process(doc, ysfx_uuid, path));
}
}

TEST_CASE("the script's state waits for the audio thread's tick", "[ysfx][threads]")
{
  YSFX::StateGuard guard;
  std::atomic_bool entered{};
  std::thread audio{[&] {
    REQUIRE(guard.try_enter());
    entered = true;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    guard.leave();
  }};
  while(!entered)
    std::this_thread::yield();

  // The audio thread does not wait
  CHECK(!guard.try_enter());

  QElapsedTimer t;
  t.start();
  {
    YSFX::StateLock lock{guard};
    CHECK(t.elapsed() >= 50);
  }
  audio.join();
  CHECK(guard.try_enter());
  guard.leave();
}

TEST_CASE(
    "a JSFX node skips the tick while its state is read or written",
    "[integration][ysfx][threads]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* proc = addScript(*doc, dir);
    if(!proc || !proc->fx)
      SKIP("YSFX not built");

    auto& plug = doc->context().plugin<Execution::DocumentPlugin>();
    plug.reload(true, score::test::base_interval(*doc));
    ossia::set_thread_pinned(ossia::thread_type::Audio, 0);
    plug.runAllCommands();
    ossia::set_thread_pinned(ossia::thread_type::Ui, 0);
    REQUIRE(plug.baseScenario());
    auto& procs = plug.baseScenario()->baseInterval().processes();
    auto it = procs.find(proc->id());
    REQUIRE(it != procs.end());
    REQUIRE(it->second->node);
    auto& node = *it->second->node;

    ossia::audio_port* out{};
    for(auto* o : node.root_outputs())
      if(auto* p = o->target<ossia::audio_port>())
        out = p;
    REQUIRE(out);

    auto& state = *plug.contextData()->execState;
    constexpr int frames = 64;
    const int setup_size = state.bufferSize;
    state.bufferSize = frames;
    const auto tick = [&] {
      ossia::token_request tk;
      tk.prev_date = ossia::time_value{0};
      tk.date = ossia::time_value{frames * 1000};
      tk.start_sample = 0;
      tk.length_sample = frames;
      out->set_channels(1);
      out->channel(0).assign(frames, 0.5);
      node.run(tk, ossia::exec_state_facade{&state});
    };
    const auto calls = [&] { return ysfx_read_var(proc->fx.get(), "calls"); };

    // Held as a preset load would hold it: the script does not run
    {
      YSFX::StateLock lock{*proc->stateGuard};
      tick();
      CHECK(calls() == 0.);
      for(double v : out->channel(0))
        REQUIRE(v == 0.);
    }

    // Released: it runs again
    tick();
    CHECK(calls() == double(frames));
    REQUIRE(!out->channel(0).empty());
    CHECK(out->channel(0)[0] == 1.);

    // A preset's state goes through the same guard: taken by the audio
    // thread, the save waits for its tick
    REQUIRE(proc->stateGuard->try_enter());
    std::thread release{[&] {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      proc->stateGuard->leave();
    }};
    QElapsedTimer t;
    t.start();
    const Process::ProcessModel& base = *proc;
    const auto preset = base.savePreset();
    CHECK(t.elapsed() >= 50);
    release.join();
    CHECK(preset.data.contains("\"State\""));

    state.bufferSize = setup_size;
  });
}
