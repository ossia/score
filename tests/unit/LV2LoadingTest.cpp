// Unit tests for LV2 plug-in loading and processing.
//
// Driven against a hermetic fixture bundle (score-test-gain.lv2, built by
// tests/fixtures/lv2) instead of whatever is installed on the system:
//  * descriptor-cache resolution (URI / bundle path / legacy display name —
//    documents saved before the URI-based chooser store the display name),
//  * on-demand bundle loading + find_lv2_plugin against the lilv world,
//  * the QSettings persistence round-trip of the scan cache,
//  * lv2_node audio processing, single-voice and per-channel multi-voice.
//
// The app boots with SCORE_DISABLE_AUDIOPLUGINS=1 (no scanner processes, no
// system bundle discovery); the URID table normally set up during startup is
// populated by calling GlobalContext::loadPlugins() explicitly.

#include <LV2/ApplicationPlugin.hpp>
#include <LV2/Context.hpp>
#include <LV2/EffectModel.hpp>
#include <LV2/Node.hpp>

#include <Media/AudioPluginCache.hpp>

#include <Process/Dataflow/WidgetInlets.hpp>

#include <score/model/EntitySerialization.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>

#include <score_test/App.hpp>

#include <ossia/network/value/value_conversion.hpp>

#include <ossia/dataflow/exec_state_facade.hpp>
#include <ossia/dataflow/execution_state.hpp>

#include <QSettings>
#include <QTemporaryDir>
#include <QUrl>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

namespace
{
constexpr auto gain_uri = "urn:score:test:gain";
constexpr auto gain_name = "Score Test Gain";

QString bundlePath()
{
  return QString::fromUtf8(SCORE_TEST_LV2_BUNDLE);
}

LV2::PluginInfo makeGainInfo()
{
  LV2::PluginInfo info;
  info.bundle = bundlePath();
  info.uri = gain_uri;
  info.name = gain_name;
  info.class_label = "Amplifier";
  info.audio_in = 1;
  info.audio_out = 1;
  info.control_in = 3;
  info.control_out = 1;
  info.valid = true;
  return info;
}

LV2::ApplicationPlugin& setupLV2(const score::GUIApplicationContext& ctx)
{
  auto& plug = ctx.applicationPlugin<LV2::ApplicationPlugin>();
  // Startup skipped this in disabled mode; populates the URID ids. The
  // LV2_PATH branch is inert since prepare_lv2_test_environment set it.
  plug.lv2_context->loadPlugins();
  plug.setCachedDescriptors({makeGainInfo()});
  return plug;
}

void prepare_lv2_test_environment()
{
  // Hermetic even when launched outside ctest: no scanner subprocesses, no
  // system LV2 directories.
  qputenv("SCORE_DISABLE_AUDIOPLUGINS", "1");
  qputenv("LV2_PATH", bundlePath().toUtf8());
}

struct noop_hook
{
  void operator()() const noexcept { }
};
using test_node = LV2::lv2_node<noop_hook, noop_hook>;

// Mirrors what LV2EffectComponent does on the main thread when execution
// starts, minus the document machinery.
struct instantiated_gain
{
  LV2::EffectContext effect;

  instantiated_gain(LV2::ApplicationPlugin& plug, int rate)
  {
    auto res = LV2::find_lv2_plugin(plug.lilv, gain_name);
    REQUIRE(res);
    effect.plugin = *res;

    auto* inst
        = lilv_plugin_instantiate(effect.plugin.me, rate, plug.lv2_host_context.features);
    REQUIRE(inst);
    effect.instance_holder = std::make_shared<LV2::InstanceHandle>(inst);
    effect.instance = inst;
  }
};

//! One tick of `samples` frames through the node.
void tick(test_node& node, ossia::execution_state& st, int64_t samples)
{
  ossia::exec_state_facade fac{&st};
  const ossia::token_request tk{
      ossia::time_value{0},       ossia::time_value{samples},
      ossia::time_value{1000000}, ossia::time_value{0},
      1.,                         ossia::time_signature{4, 4},
      120.};
  static_cast<ossia::graph_node&>(node).run(tk, fac);
}
}

TEST_CASE("LV2 descriptor cache lookup", "[lv2]")
{
  prepare_lv2_test_environment();
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto& plug = ctx.applicationPlugin<LV2::ApplicationPlugin>();

    LV2::PluginInfo other;
    other.bundle = "/nonexistent/other.lv2";
    other.uri = "urn:score:test:other";
    other.name = gain_uri; // adversarial: name colliding with another URI
    other.valid = true;

    LV2::PluginInfo invalid; // <Invalid>/<Empty> markers must never resolve
    invalid.bundle = "/nonexistent/broken.lv2";
    invalid.name = "Ghost";
    invalid.valid = false;

    plug.setCachedDescriptors({invalid, other, makeGainInfo()});

    SECTION("by URI")
    {
      auto* info = plug.findDescriptor(gain_uri);
      REQUIRE(info);
      CHECK(info->name == gain_name);
      // URI match takes precedence over a name equal to that URI
      CHECK(info->uri == gain_uri);
    }
    SECTION("by bundle path")
    {
      REQUIRE(plug.findDescriptor(bundlePath()));
      REQUIRE(plug.findDescriptor(bundlePath() + "/"));
      REQUIRE(plug.findDescriptor(QUrl::fromLocalFile(bundlePath()).toString()));
    }
    SECTION("by legacy display name")
    {
      // Documents saved before the URI-based chooser store the display name
      auto* info = plug.findDescriptor(gain_name);
      REQUIRE(info);
      CHECK(info->uri == gain_uri);
    }
    SECTION("misses")
    {
      CHECK(plug.findDescriptor("No Such Plugin") == nullptr);
      CHECK(plug.findDescriptor("Ghost") == nullptr);
      CHECK(plug.findDescriptor("") == nullptr);
    }
  });
}

TEST_CASE("find_lv2_plugin loads the bundle on demand", "[lv2]")
{
  prepare_lv2_test_environment();
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto& plug = setupLV2(ctx);

    SECTION("by legacy display name")
    {
      auto res = LV2::find_lv2_plugin(plug.lilv, gain_name);
      REQUIRE(res);
      CHECK(LV2::get_lv2_plugin_name(*res) == gain_name);
      CHECK(QString(res->get_uri().as_string()) == gain_uri);

      // Cached hit returns the same plugin
      auto again = LV2::find_lv2_plugin(plug.lilv, gain_name);
      REQUIRE(again);
      CHECK(again->me == res->me);
    }
    SECTION("by URI")
    {
      auto res = LV2::find_lv2_plugin(plug.lilv, gain_uri);
      REQUIRE(res);
      CHECK(QString(res->get_uri().as_string()) == gain_uri);
    }
    SECTION("by bundle file URI")
    {
      auto res
          = LV2::find_lv2_plugin(plug.lilv, QUrl::fromLocalFile(bundlePath()).toString());
      REQUIRE(res);
      CHECK(QString(res->get_uri().as_string()) == gain_uri);
    }
    SECTION("miss")
    {
      CHECK(!LV2::find_lv2_plugin(plug.lilv, "No Such Plugin"));
    }
    SECTION("descriptor pin controls which same-named plug-in binds")
    {
      // The bundle ships urn:score:test:gain2 with the same doap:name. When
      // the descriptor cache pins the name to gain2, the lookup must return
      // gain2 no matter which of the two iterates first in the world.
      auto info2 = makeGainInfo();
      info2.uri = "urn:score:test:gain2";
      plug.setCachedDescriptors({info2});

      auto res = LV2::find_lv2_plugin(plug.lilv, gain_name);
      REQUIRE(res);
      CHECK(QString(res->get_uri().as_string()) == "urn:score:test:gain2");
    }
  });
}

TEST_CASE("LV2 descriptor cache round-trips through the versioned blob", "[lv2]")
{
  prepare_lv2_test_environment();
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    const std::vector<LV2::PluginInfo> src{makeGainInfo()};

    const auto blob = Media::serializePluginCache(1, src);
    const auto back = Media::deserializePluginCache<LV2::PluginInfo>(1, blob);
    REQUIRE(back.has_value());
    REQUIRE(back->size() == 1);
    CHECK((*back)[0].bundle == src[0].bundle);
    CHECK((*back)[0].uri == src[0].uri);
    CHECK((*back)[0].control_in == src[0].control_in);
    CHECK((*back)[0].valid == src[0].valid);

    // A format bump must invalidate cleanly, not decode garbage
    CHECK(!Media::deserializePluginCache<LV2::PluginInfo>(2, blob).has_value());
  });
}

TEST_CASE("LV2 descriptor cache round-trips through QSettings", "[lv2]")
{
  prepare_lv2_test_environment();
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString file = dir.path() + "/cache.ini";

    const std::vector<LV2::PluginInfo> src{makeGainInfo()};
    {
      QSettings s{file, QSettings::IniFormat};
      s.setValue("Effect/KnownLV2", QVariant::fromValue(src));
    }

    QSettings s{file, QSettings::IniFormat};
    auto val = s.value("Effect/KnownLV2");
    REQUIRE(val.canConvert<std::vector<LV2::PluginInfo>>());
    auto loaded = val.value<std::vector<LV2::PluginInfo>>();
    REQUIRE(loaded.size() == 1);

    const auto& a = src[0];
    const auto& b = loaded[0];
    CHECK(a.bundle == b.bundle);
    CHECK(a.uri == b.uri);
    CHECK(a.name == b.name);
    CHECK(a.class_label == b.class_label);
    CHECK(a.audio_in == b.audio_in);
    CHECK(a.audio_out == b.audio_out);
    CHECK(a.control_in == b.control_in);
    CHECK(a.control_out == b.control_out);
    CHECK(a.valid == b.valid);
  });
}

TEST_CASE("LV2::Model resolves a legacy name-based document reference", "[lv2]")
{
  prepare_lv2_test_environment();
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto& plug = setupLV2(ctx);
    (void)plug;

    QObject parent;
    LV2::Model model{
        TimeVal::fromMsecs(1000), gain_name, Id<Process::ProcessModel>{1}, &parent};

    REQUIRE(model.plugin);
    CHECK(model.effect() == gain_name);
    // audio in + gain/mode/bypass controls
    CHECK(model.inlets().size() == 4);
    // audio out + level control
    CHECK(model.outlets().size() == 2);
  });
}

TEST_CASE("LV2 control values survive a reload round-trip", "[lv2]")
{
  prepare_lv2_test_environment();
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    setupLV2(ctx);

    QObject parent;
    auto* model = new LV2::Model{
        TimeVal::fromMsecs(1000), gain_name, Id<Process::ProcessModel>{1}, &parent};
    REQUIRE(model->plugin);
    REQUIRE(model->inlets().size() == 4);

    // Port properties picked the matching widgets
    auto* gain_p = dynamic_cast<Process::FloatSlider*>(model->inlets()[1]);
    auto* mode_p = dynamic_cast<Process::IntSlider*>(model->inlets()[2]);
    auto* byp_p = dynamic_cast<Process::Toggle*>(model->inlets()[3]);
    REQUIRE(gain_p);
    REQUIRE(mode_p);
    REQUIRE(byp_p);

    gain_p->setValue(0.25f);
    mode_p->setValue(2);
    byp_p->setValue(true);

    JSONReader reader;
    reader.readFrom(static_cast<const Process::ProcessModel&>(*model));
    const auto doc = toValue(reader);

    JSONObject::Deserializer des{doc};
    auto* loaded = new LV2::Model{des, &parent};
    REQUIRE(loaded->plugin);
    REQUIRE(loaded->inlets().size() == 4);

    CHECK(
        ossia::convert<float>(
            dynamic_cast<Process::ControlInlet*>(loaded->inlets()[1])->value())
        == Catch::Approx(0.25));
    CHECK(
        ossia::convert<int>(
            dynamic_cast<Process::ControlInlet*>(loaded->inlets()[2])->value())
        == 2);
    CHECK(
        ossia::convert<bool>(
            dynamic_cast<Process::ControlInlet*>(loaded->inlets()[3])->value())
        == true);
  });
}

TEST_CASE("LV2 documents with legacy all-FloatSlider controls keep values", "[lv2]")
{
  // Before port properties were honored, every control inlet was saved as a
  // FloatSlider. Reloading such a document must not reset ports whose widget
  // type changed (IntSlider / Toggle / ...) to their default value.
  prepare_lv2_test_environment();
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    setupLV2(ctx);

    QObject parent;
    auto* model = new LV2::Model{
        TimeVal::fromMsecs(1000), gain_name, Id<Process::ProcessModel>{1}, &parent};
    REQUIRE(model->plugin);
    REQUIRE(model->inlets().size() == 4);

    dynamic_cast<Process::ControlInlet*>(model->inlets()[1])->setValue(0.25f);
    dynamic_cast<Process::ControlInlet*>(model->inlets()[2])->setValue(2);
    dynamic_cast<Process::ControlInlet*>(model->inlets()[3])->setValue(true);

    JSONReader reader;
    reader.readFrom(static_cast<const Process::ProcessModel&>(*model));
    auto doc = toValue(reader);

    // Rewrite the mode/bypass inlet entries to the FloatSlider uuid + float
    // values, as an old document would have stored them
    const std::string fs_uuid = [] {
      Process::FloatSlider scratch{0.f, 1.f, 0.f, "s", Id<Process::Port>{99}, nullptr};
      const auto sj = toValue(score::marshall<JSONObject>(
          static_cast<const Process::Inlet&>(scratch)));
      REQUIRE(sj.HasMember("uuid"));
      REQUIRE(sj["uuid"].IsString());
      return std::string{sj["uuid"].GetString()};
    }();

    REQUIRE(doc.HasMember("Inlets"));
    auto inlets = doc["Inlets"].GetArray();
    REQUIRE(inlets.Size() == 4);
    int patched = 0;
    for(rapidjson::SizeType i = 2; i < 4; ++i)
    {
      auto entry = inlets[i].GetObject();
      REQUIRE(entry.HasMember("uuid"));
      REQUIRE(entry.HasMember("Value"));
      entry["uuid"].SetString(fs_uuid.c_str(), doc.GetAllocator());
      // ossia::value serializes as a typed object; a FloatSlider stores Float
      rapidjson::Value v{rapidjson::kObjectType};
      v.AddMember("Float", i == 2 ? 2.0 : 1.0, doc.GetAllocator());
      entry["Value"] = v;
      patched++;
    }
    REQUIRE(patched == 2);

    JSONObject::Deserializer des{doc};
    auto* loaded = new LV2::Model{des, &parent};
    REQUIRE(loaded->plugin);
    REQUIRE(loaded->inlets().size() == 4);

    // Values survive even though the current widgets are IntSlider / Toggle
    CHECK(
        ossia::convert<float>(
            dynamic_cast<Process::ControlInlet*>(loaded->inlets()[1])->value())
        == Catch::Approx(0.25));
    CHECK(
        ossia::convert<float>(
            dynamic_cast<Process::ControlInlet*>(loaded->inlets()[2])->value())
        == Catch::Approx(2.0));
    CHECK(
        ossia::convert<float>(
            dynamic_cast<Process::ControlInlet*>(loaded->inlets()[3])->value())
        == Catch::Approx(1.0));
  });
}

TEST_CASE("lv2_node processes audio single-voice", "[lv2]")
{
  prepare_lv2_test_environment();
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto& plug = setupLV2(ctx);
    instantiated_gain gain{plug, 48000};

    LV2::LV2Data data{plug.lv2_host_context, gain.effect};
    REQUIRE(data.audio_in_ports.size() == 1);
    REQUIRE(data.audio_out_ports.size() == 1);
    REQUIRE(data.control_in_ports.size() == 3);
    REQUIRE(data.control_out_ports.size() == 1);

    {
      test_node node{data, 48000, {LV2::voice_routing::single, 1}, {}, {}};
      REQUIRE(node.voices.size() == 1);
      REQUIRE(node.root_inputs().size() == 4);  // audio, gain, mode, bypass
      REQUIRE(node.root_outputs().size() == 2); // audio, level

      auto& in = node.root_inputs()[0]->cast<ossia::audio_port>();
      in.set_channels(1);
      constexpr int64_t N = 64;
      auto& ch = in.channel(0);
      ch.resize(N);
      for(int64_t i = 0; i < N; i++)
        ch[i] = double(i) / N;

      ossia::execution_state st;

      SECTION("default gain of 1 passes through")
      {
        tick(node, st, N);

        auto& out = node.root_outputs()[0]->cast<ossia::audio_port>();
        REQUIRE(out.channels() == 1);
        REQUIRE(out.channel(0).size() == std::size_t(N));
        for(int64_t i = 0; i < N; i++)
          CHECK(out.channel(0)[i] == Catch::Approx(double(i) / N).margin(1e-6));

        // control out surfaces voice 0's level: peak of the ramp
        auto& level = node.root_outputs()[1]->cast<ossia::value_port>();
        REQUIRE(!level.get_data().empty());
        CHECK(
            ossia::convert<float>(level.get_data().back().value)
            == Catch::Approx(double(N - 1) / N).margin(1e-6));
      }

      SECTION("control inlet scales the output")
      {
        node.root_inputs()[1]->cast<ossia::value_port>().write_value(0.5f, 0);
        tick(node, st, N);

        auto& out = node.root_outputs()[0]->cast<ossia::audio_port>();
        REQUIRE(out.channels() == 1);
        for(int64_t i = 0; i < N; i++)
          CHECK(
              out.channel(0)[i] == Catch::Approx(0.5 * double(i) / N).margin(1e-6));
      }

      SECTION("zero-length tick is skipped")
      {
        tick(node, st, 0);
        // paused() short-circuits: no samples written to any channel
        auto& out = node.root_outputs()[0]->cast<ossia::audio_port>();
        for(std::size_t c = 0; c < out.channels(); c++)
          CHECK(out.channel(c).empty());
        auto& level = node.root_outputs()[1]->cast<ossia::value_port>();
        CHECK(level.get_data().empty());
      }
    }
    // ~lv2_node defers voice teardown to the main thread
    QCoreApplication::processEvents();
  });
}

TEST_CASE("lv2_node replicates voices per input channel", "[lv2]")
{
  prepare_lv2_test_environment();
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto& plug = setupLV2(ctx);
    instantiated_gain gain{plug, 48000};

    LV2::LV2Data data{plug.lv2_host_context, gain.effect};

    // 1-in/1-out picks the per-channel strategy
    const auto strat = LV2::choose_voice_strategy(data, 4);
    REQUIRE(strat.routing == LV2::voice_routing::per_channel);
    REQUIRE(strat.voice_count == 4);

    {
      test_node node{data, 48000, strat, {}, {}};
      REQUIRE(node.voices.size() == 4);

      constexpr int64_t N = 32;
      auto& in = node.root_inputs()[0]->cast<ossia::audio_port>();
      in.set_channels(2);
      for(int c = 0; c < 2; c++)
      {
        auto& ch = in.channel(c);
        ch.assign(N, c == 0 ? 1.0 : -0.25);
      }

      // Per-voice gains: a list value maps element i to voice i
      node.root_inputs()[1]->cast<ossia::value_port>().write_value(
          std::vector<ossia::value>{0.5f, 2.0f}, 0);

      ossia::execution_state st;
      tick(node, st, N);

      auto& out = node.root_outputs()[0]->cast<ossia::audio_port>();
      // Only as many voices as input channels are active
      REQUIRE(out.channels() == 2);
      for(int64_t i = 0; i < N; i++)
      {
        CHECK(out.channel(0)[i] == Catch::Approx(0.5).margin(1e-6));
        CHECK(out.channel(1)[i] == Catch::Approx(-0.5).margin(1e-6));
      }
    }
    QCoreApplication::processEvents();
  });
}
