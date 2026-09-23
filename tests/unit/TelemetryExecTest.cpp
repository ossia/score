// Execution::Telemetry: the meters views ask for reach the running graph, come
// back at the document's coarse rate, and stop with the execution-update
// setting.
//
// The graph is built with DocumentPlugin::reload and driven by calling the
// execution tick by hand, the main thread posing as the audio thread.

#include <Process/Dataflow/Port.hpp>

#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Settings/ScenarioSettingsModel.hpp>

#include <Audio/AudioApplicationPlugin.hpp>
#include <Execution/DocumentPlugin.hpp>
#include <Execution/ExecutionTick.hpp>
#include <Execution/Telemetry.hpp>

#include <core/document/Document.hpp>

#include <ossia/audio/audio_engine.hpp>
#include <ossia/detail/thread.hpp>

#include <QApplication>
#include <QElapsedTimer>

#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

#include <array>

namespace
{
constexpr int frames = 64;

struct AsAudioThread
{
  AsAudioThread() { ossia::set_thread_pinned(ossia::thread_type::Audio, 0); }
  ~AsAudioThread() { ossia::set_thread_pinned(ossia::thread_type::Ui, 0); }
};

void run_exec(Execution::DocumentPlugin& plug)
{
  AsAudioThread audio;
  plug.runAllCommands();
}

// Waits for the telemetry to hand over a frame, up to a timeout.
bool wait_update(Execution::Telemetry& t, int ms = 3000)
{
  bool updated = false;
  auto con = QObject::connect(&t, &Execution::Telemetry::updated, [&] { updated = true; });
  QElapsedTimer timer;
  timer.start();
  while(!updated && timer.elapsed() < ms)
    QApplication::processEvents(QEventLoop::AllEvents, 5);
  QObject::disconnect(con);
  return updated;
}

struct Card
{
  std::array<std::array<float, frames>, 2> buffers{};
  std::array<float*, 2> outputs{buffers[0].data(), buffers[1].data()};

  ossia::audio_tick_state state()
  {
    return {.inputs = nullptr, .outputs = outputs.data(), .n_in = 0, .n_out = 2,
            .frames = frames};
  }
};

// The running audio engine must not tick the actions while the test does.
void park_audio_engine(const score::GUIApplicationContext& ctx)
{
  auto& audio = ctx.guiApplicationPlugin<Audio::ApplicationPlugin>();
  if(audio.audio)
    audio.audio->set_tick([](const ossia::audio_tick_state&) {});
}

// Enough callbacks to cover any publication interval.
void play(Execution::tick_fun& tick, Card& card, int callbacks = 400)
{
  AsAudioThread audio;
  for(int i = 0; i < callbacks; i++)
    tick(card.state());
}
}

TEST_CASE("Requested meters come back from the execution", "[telemetry][execution]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    park_audio_engine(ctx);
    auto& plug = doc->context().plugin<Execution::DocumentPlugin>();
    auto& telemetry = plug.telemetry();
    auto& root = score::test::base_interval(*doc);

    auto hw = telemetry.meterHardwareOutputs();
    auto out = telemetry.meterOutlet(*root.outlet);
    REQUIRE(hw);
    REQUIRE(out);

    // Nothing runs yet.
    CHECK(!telemetry.levels(hw));

    plug.reload(true, root);
    run_exec(plug);
    auto tick = Execution::makeExecutionTick({}, plug, plug.baseScenario());

    Card card;
    play(tick, card);
    REQUIRE(wait_update(telemetry));

    const auto* levels = telemetry.levels(hw);
    REQUIRE(levels);
    CHECK(levels->channels == 2);
    CHECK(levels->ticks > 0);
    CHECK(levels->peak[0] == 0.f);
    CHECK(telemetry.sampleRate() > 0);

    // The root interval runs; an empty score carries no channel.
    const auto* root_levels = telemetry.levels(out);
    REQUIRE(root_levels);
    CHECK(root_levels->ticks > 0);

    SECTION("a released meter reads as nothing")
    {
      telemetry.release(out);
      CHECK(!telemetry.levels(out));
    }

    SECTION("updates stop with the execution-update setting")
    {
      auto& settings = ctx.settings<Scenario::Settings::Model>();
      settings.setExecutionUpdate(false);
      run_exec(plug);
      CHECK(!telemetry.levels(hw));

      play(tick, card);
      CHECK(!wait_update(telemetry, 400));

      settings.setExecutionUpdate(true);
      run_exec(plug);
      play(tick, card);
      REQUIRE(wait_update(telemetry));
      CHECK(telemetry.levels(hw));
    }

    SECTION("stopping clears the meters, and they come back on the next run")
    {
      plug.clear();
      CHECK(!telemetry.levels(hw));

      plug.reload(true, root);
      run_exec(plug);
      auto tick2 = Execution::makeExecutionTick({}, plug, plug.baseScenario());
      play(tick2, card);
      REQUIRE(wait_update(telemetry));
      CHECK(telemetry.levels(hw));
      CHECK(telemetry.levels(out));
    }
  });
}
