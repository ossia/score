// Execution::Telemetry: the meters views ask for reach the running graph, come
// back at the document's coarse rate, and stop with the execution-update
// setting.
//
// The graph is built with DocumentPlugin::reload and driven by calling the
// execution tick by hand, the main thread posing as the audio thread.

#include <Process/Dataflow/Port.hpp>

#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Settings/ScenarioSettingsModel.hpp>

#include <Explorer/DeviceList.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Audio/AudioApplicationPlugin.hpp>
#include <Audio/AudioDevice.hpp>
#include <Execution/DocumentPlugin.hpp>
#include <Execution/ExecutionTick.hpp>
#include <Execution/Telemetry.hpp>

#include <core/document/Document.hpp>

#include <ossia/audio/audio_engine.hpp>
#include <ossia/audio/audio_parameter.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/network/base/node_functions.hpp>
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

TEST_CASE("A virtual port is metered, until it is removed", "[telemetry][execution]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    park_audio_engine(ctx);
    auto& plug = doc->context().plugin<Execution::DocumentPlugin>();
    auto& telemetry = plug.telemetry();
    auto& root = score::test::base_interval(*doc);

    auto* dev = static_cast<Dataflow::AudioDevice*>(
        doc->context().plugin<Explorer::DeviceDocumentPlugin>().list().audioDevice());
    REQUIRE(dev);
    Device::FullAddressSettings bus;
    bus.address = State::Address{"audio", {"bus"}};
    bus.extendedAttributes["audio-kind"] = std::string{"virtual"};
    bus.extendedAttributes["audio-channels"] = 2;
    dev->addAddress(bus);
    auto* node = ossia::net::find_node(dev->getDevice()->get_root_node(), "/bus");
    REQUIRE(node);
    auto* param = dynamic_cast<ossia::virtual_audio_parameter*>(node->get_parameter());
    REQUIRE(param);

    auto meter = telemetry.meterVirtualPort(*param);
    REQUIRE(meter);

    plug.reload(true, root);
    run_exec(plug);
    auto tick = Execution::makeExecutionTick({}, plug, plug.baseScenario());

    // What a process writing to audio:/bus does in each tick.
    ossia::audio_port written;
    written.set_channels(2);
    written.channel(0).assign(frames, 0.5);
    written.channel(1).assign(frames, 0.25);

    Card card;
    {
      AsAudioThread audio;
      for(int i = 0; i < 400; i++)
      {
        param->push_value(written);
        tick(card.state());
      }
    }
    REQUIRE(wait_update(telemetry));
    const auto* levels = telemetry.levels(meter);
    REQUIRE(levels);
    CHECK(levels->channels == 2);
    CHECK(levels->peak[0] == 0.5f);
    CHECK(levels->peak[1] == 0.25f);

    // The port goes away while metered: the tap is let go first.
    dev->removeNode(bus.address);
    CHECK(!ossia::net::find_node(dev->getDevice()->get_root_node(), "/bus"));
    run_exec(plug);
    play(tick, card);
    REQUIRE(wait_update(telemetry));
    CHECK(!telemetry.levels(meter));
    telemetry.release(meter);
  });
}

namespace
{
ossia::outlet* execOutlet(Execution::DocumentPlugin& plug, Process::Outlet& outlet)
{
  auto& outlets = plug.contextData()->setupContext.outlets;
  auto it = outlets.find(&outlet);
  return it != outlets.end() ? it->second.second : nullptr;
}

ossia::net::parameter_base* destination(const ossia::outlet& out)
{
  auto p = out.address.target<ossia::net::parameter_base*>();
  return p ? *p : nullptr;
}
}

TEST_CASE(
    "Removing ports while playing lets go of them once, without stopping the audio",
    "[telemetry][execution]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    park_audio_engine(ctx);
    auto& plug = doc->context().plugin<Execution::DocumentPlugin>();
    auto& telemetry = plug.telemetry();
    auto& root = score::test::base_interval(*doc);
    auto* dev = static_cast<Dataflow::AudioDevice*>(
        doc->context().plugin<Explorer::DeviceDocumentPlugin>().list().audioDevice());
    REQUIRE(dev);

    // Many ports under one node, a nested one metered, one written to.
    auto add_ports = [&] {
      for(int i = 0; i < 200; i++)
      {
        Device::FullAddressSettings s;
        s.address = State::Address{"audio", {"grp", QString("v%1").arg(i)}};
        s.extendedAttributes["audio-kind"] = std::string{"virtual"};
        s.extendedAttributes["audio-channels"] = 2;
        dev->addAddress(s);
      }
    };
    auto find_param = [&](const std::string& path) {
      auto node = ossia::net::find_node(dev->getDevice()->get_root_node(), path);
      return node ? dynamic_cast<ossia::virtual_audio_parameter*>(node->get_parameter())
                  : nullptr;
    };
    root.outlet->setAddress(State::AddressAccessor{State::Address{"audio", {"grp", "v0"}}});

    auto remove_while_playing = [&] {
      auto* written = find_param("/grp/v0");
      auto* metered = find_param("/grp/v7");
      REQUIRE(written);
      REQUIRE(metered);
      auto meter = telemetry.meterVirtualPort(*metered);

      plug.reload(true, root);
      run_exec(plug);
      auto tick = Execution::makeExecutionTick({}, plug, plug.baseScenario());
      Card card;
      play(tick, card, 50);
      auto* out = execOutlet(plug, *root.outlet);
      REQUIRE(out);
      CHECK(destination(*out) == written);

      // One wait of about a buffer for the whole node, not one per port.
      QElapsedTimer t;
      t.start();
      dev->removeNode(State::Address{"audio", {"grp"}});
      CHECK(t.elapsed() < 1000);
      CHECK(!find_param("/grp/v0"));

      CHECK(destination(*out) == nullptr);
      play(tick, card, 50);
      CHECK(!telemetry.levels(meter));
      telemetry.release(meter);
      plug.clear();
    };

    add_ports();
    remove_while_playing();

    SECTION("after the audio device was rebuilt")
    {
      REQUIRE(dev->reconnect());
      add_ports();
      remove_while_playing();
    }
  });
}

TEST_CASE("An outlet's gain, pan and upmix reach the running outlet", "[execution][mixing]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    park_audio_engine(ctx);
    auto& plug = doc->context().plugin<Execution::DocumentPlugin>();
    auto& root = score::test::base_interval(*doc);
    root.outlet->setGain(0.5);

    plug.reload(true, root);
    run_exec(plug);
    auto* out = static_cast<ossia::audio_outlet*>(execOutlet(plug, *root.outlet));
    REQUIRE(out);
    CHECK(out->gain == 0.5);

    root.outlet->setGain(0.25);
    root.outlet->setPan({0.2, 0.8});
    root.outlet->setUpmixMode(1);
    root.outlet->setUpmixChannels(4);
    run_exec(plug);
    CHECK(out->gain == 0.25);
    REQUIRE(out->pan.size() == 2);
    CHECK(out->pan[0] == 0.2);
    CHECK(out->upmix == ossia::audio_outlet::upmix_mode(1));
    CHECK(out->upmix_channels == 4);
    plug.clear();
  });
}
