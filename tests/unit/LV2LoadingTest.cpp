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
#include <LV2/Window.hpp>

#include <Audio/Settings/Model.hpp>
#include <Media/AudioPluginCache.hpp>

#include <Process/Dataflow/WidgetInlets.hpp>
#include <Process/Preset.hpp>
#include <Process/ProcessList.hpp>

#include <score/model/EntitySerialization.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>

#include <score_test/App.hpp>

#include <ossia/network/value/value_conversion.hpp>

#include <libremidi/detail/conversion.hpp>

#include <ossia/dataflow/exec_state_facade.hpp>
#include <ossia/dataflow/execution_state.hpp>

#include <QFile>
#include <QSettings>
#include <QTemporaryDir>
#include <QUrl>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstring>
#if defined(__linux__)
#include <dlfcn.h>
#endif
#include <functional>
#include <map>

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

TEST_CASE("LV2 UI binaries linked against another Qt major are rejected", "[lv2]")
{
  // Identical mangled symbol names across Qt majors get cross-resolved when
  // such a UI is dlopened into this process, corrupting memory before the
  // UI even finishes creating its QApplication (observed with qmidiarp's
  // Qt5 X11UI: dev builds die on a QStringView assert, static-Qt release
  // builds corrupt silently since the executable exports its Qt).
  QTemporaryDir dir;
  REQUIRE(dir.isValid());

  auto write = [&](const char* name, QByteArray content) {
    const QString p = dir.path() + "/" + name;
    QFile f{p};
    REQUIRE(f.open(QIODevice::WriteOnly));
    f.write(content);
    return p;
  };

  const auto qt5_ui
      = write("qt5_ui.so", "\x7f" "ELF...libQt5Core.so.5...libQt5Gui.so.5...");
  const auto qt4_ui = write("qt4_ui.so", "...libQt4Gui.so.4...");
  const auto our_qt_ui = write(
      "qt6_ui.so",
      QByteArray("...libQt") + QByteArray::number(QT_VERSION_MAJOR)
          + "Core.so...");
  const auto no_qt_ui = write("plain_ui.so", "...libgtk-3.so.0...libX11.so...");

  CHECK(LV2::uiLinksIncompatibleQt(qt5_ui));
  CHECK(LV2::uiLinksIncompatibleQt(qt4_ui));
  CHECK(!LV2::uiLinksIncompatibleQt(our_qt_ui));
  CHECK(!LV2::uiLinksIncompatibleQt(no_qt_ui));
  CHECK(!LV2::uiLinksIncompatibleQt(dir.path() + "/does_not_exist.so"));

  // macOS / Windows dependency name shapes go through the generic scan
  const auto dylib_ui = write("qt5_mac.so", "...libQt5Gui.5.dylib...");
  const auto fw_ui = write(
      "qt5_fw.so", "...@rpath/QtGui.framework/Versions/5/QtGui...");
  const auto dll_ui = write("qt5_win.so", "...Qt5Gui.dll...");
  CHECK(LV2::uiLinksIncompatibleQt(dylib_ui));
  CHECK(LV2::uiLinksIncompatibleQt(fw_ui));
  CHECK(LV2::uiLinksIncompatibleQt(dll_ui));

#if defined(__linux__)
  // The definitive DT_NEEDED path, against real ELFs: our own Qt6-linked
  // plug-in must be accepted...
  CHECK(!LV2::uiLinksIncompatibleQt(QStringLiteral(SCORE_TEST_QT6_ELF)));
  // ...and a real Qt5-linked library rejected, when one is installed
  if(QFile::exists("/usr/lib/libQt5Gui.so.5"))
    CHECK(LV2::uiLinksIncompatibleQt("/usr/lib/libQt5Gui.so.5"));
#endif
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

    // Truncation must be rejected *for a real score-serialized type*: the
    // score datastream operators route reads through an internal stream, so
    // a naive status check on the outer stream never fires and truncated
    // blobs used to decode as garbage (the failure mode the old VST2
    // vst_invalid_format global caught).
    for(int cut : {1, 8, 12, 20, (int)blob.size() / 2, (int)blob.size() - 1})
    {
      auto truncated = blob;
      truncated.truncate(cut);
      CHECK(
          !Media::deserializePluginCache<LV2::PluginInfo>(1, truncated).has_value());
    }
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

namespace
{
struct fn_hook
{
  std::function<void()> fn;
  void operator()() const noexcept
  {
    if(fn)
      fn();
  }
};
using hooked_node = LV2::lv2_node<fn_hook, fn_hook>;

//! A fixture plug-in of the test bundle, instantiated as the Model does.
struct fixture_plugin
{
  LV2::EffectContext effect;
  fixture_plugin(
      LV2::ApplicationPlugin& plug, const char* uri, const char* name, bool midi = false)
  {
    LV2::PluginInfo info;
    info.bundle = bundlePath();
    info.uri = uri;
    info.name = name;
    info.class_label = "Utility";
    info.midi_in = midi;
    info.midi_out = midi;
    info.valid = true;
    plug.setCachedDescriptors({makeGainInfo(), info});
    auto res = LV2::find_lv2_plugin(plug.lilv, uri);
    REQUIRE(res);
    effect.plugin = *res;
    auto* inst = lilv_plugin_instantiate(
        effect.plugin.me, 48000, plug.lv2_context->features());
    REQUIRE(inst);
    effect.instance_holder = std::make_shared<LV2::InstanceHandle>(inst);
    effect.instance = inst;
  }
};

template <typename Node>
void run_node(Node& node, ossia::execution_state& st, bool playing = true)
{
  ossia::exec_state_facade fac{&st};
  ossia::token_request tk{};
  if(playing)
  {
    tk = ossia::token_request{
        ossia::time_value{0},       ossia::time_value{64},
        ossia::time_value{1000000}, ossia::time_value{0},
        1.,                         ossia::time_signature{4, 4},
        120.};
    tk.start_sample = 0;
    tk.length_sample = 64;
  }
  static_cast<ossia::graph_node&>(node).run(tk, fac);
}
}

// Atom ports that ask for rsz:minimumSize 65536 get it: a plug-in sends its
// whole state to a UI that opens, and a smaller buffer drops it. The fixture
// plug-in answers each input event with a 10000-byte event.
TEST_CASE("lv2_node gives atom ports the buffer size they ask for", "[lv2]")
{
  prepare_lv2_test_environment();
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto& plug = setupLV2(ctx);
    fixture_plugin p{plug, "urn:score:test:notify", "Score Test Notify", true};
    LV2::LV2Data data{plug.lv2_host_context, p.effect};
    REQUIRE(data.midi_in_ports.size() == 1);
    REQUIRE(data.midi_out_ports.size() == 1);
    CHECK(data.atomBufferSize(data.midi_in_ports[0]) >= 65536);
    CHECK(data.atomBufferSize(data.midi_out_ports[0]) >= 65536);

    {
      hooked_node node{data, 48000, {LV2::voice_routing::single, 1}, {}, {}};
      REQUIRE(node.voices.size() == 1);

      // What on_finish forwards to the UI: every event of the output port.
      std::vector<uint32_t> sent;
      node.on_finished.fn = [&] {
        LV2_ATOM_SEQUENCE_FOREACH(&node.voices[0]->midi_atom_outs[0].buf->atoms, ev)
          sent.push_back(ev->body.size);
      };

      // A message from the UI, as on_start queues it
      LV2::Message msg;
      msg.index = data.midi_in_ports[0];
      msg.protocol = plug.lv2_host_context.atom_eventTransfer;
      const uint8_t note_on[3]{0x90, 60, 100};
      msg.body.resize(sizeof(LV2_Atom) + 3);
      auto* atom = reinterpret_cast<LV2_Atom*>(msg.body.data());
      atom->type = plug.lv2_host_context.midi_event_id;
      atom->size = 3;
      std::memcpy(msg.body.data() + sizeof(LV2_Atom), note_on, 3);
      node.voices[0]->message_for_midi_atom_ins[0].push_back(msg);

      ossia::execution_state st;
      run_node(node, st);
      REQUIRE(sent.size() == 1);
      CHECK(sent[0] == 10000u);
    }
    QCoreApplication::processEvents();
  });
}

// A sequencer following the host transport starts on a change of time:speed
// from 0. The instance lives from one play to the next, so the transport
// stopping must be told to it.
TEST_CASE("lv2_node tells the plug-in the transport stopped and started", "[lv2]")
{
  prepare_lv2_test_environment();
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto& plug = setupLV2(ctx);
    fixture_plugin p{plug, "urn:score:test:transport", "Score Test Transport"};
    LV2::LV2Data data{plug.lv2_host_context, p.effect};
    REQUIRE(data.time_Position_ports.size() == 1);
    REQUIRE(data.control_out_ports.size() == 2);
    ossia::execution_state st;
    auto speed = [](test_node& n) { return n.voices[0]->fOutControls[0]; };
    auto starts = [](test_node& n) { return n.voices[0]->fOutControls[1]; };

    {
      test_node node{data, 48000, {LV2::voice_routing::single, 1}, {}, {}};
      run_node(node, st);
      run_node(node, st);
      CHECK(speed(node) == 1.f);
      CHECK(starts(node) == 1.f);

      // Stop: what the process does, then the tick it requests.
      node.all_notes_off();
      node.mustStop = true;
      run_node(node, st, false);
      CHECK(speed(node) == 0.f);
    }
    QCoreApplication::processEvents();

    // Play again: the same instance hears it start again.
    {
      test_node node{data, 48000, {LV2::voice_routing::single, 1}, {}, {}};
      run_node(node, st);
      CHECK(speed(node) == 1.f);
      CHECK(starts(node) == 2.f);
    }
    QCoreApplication::processEvents();

    // A play that ended without a stop tick (a crash, a reload): the first
    // run still starts from "stopped".
    {
      test_node node{data, 48000, {LV2::voice_routing::single, 1}, {}, {}};
      run_node(node, st);
      CHECK(starts(node) == 3.f);
    }
    QCoreApplication::processEvents();
  });
}

// A plug-in without activate() cannot be restarted through it: a play starts
// on a fresh instance, with the state of the previous one.
TEST_CASE("lv2_node restarts a plug-in that has no activate()", "[lv2]")
{
  prepare_lv2_test_environment();
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto& plug = setupLV2(ctx);
    fixture_plugin p{plug, "urn:score:test:counter", "Score Test Counter"};
    LV2::LV2Data data{plug.lv2_host_context, p.effect};
    REQUIRE(data.control_out_ports.size() == 1);
    ossia::execution_state st;

    {
      test_node node{data, 48000, {LV2::voice_routing::single, 1}, {}, {}};
      for(int i = 0; i < 3; i++)
        run_node(node, st);
      CHECK(node.voices[0]->fOutControls[0] == 3.f);
    }
    QCoreApplication::processEvents();
    {
      test_node node{data, 48000, {LV2::voice_routing::single, 1}, {}, {}};
      run_node(node, st);
      CHECK(node.voices[0]->fOutControls[0] == 1.f);
      // The Model follows: it holds the instance that plays.
      CHECK(p.effect.instance == node.voices[0]->instance);
    }
    QCoreApplication::processEvents();
  });
}

// A CV output is an outlet, and a control with a minimum but no default nor
// maximum does not show as nan.
TEST_CASE("LV2 CV outputs are outlets, missing ranges are filled in", "[lv2]")
{
  prepare_lv2_test_environment();
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto& plug = setupLV2(ctx);
    fixture_plugin p{plug, "urn:score:test:envelope", "Score Test Envelope"};
    LV2::LV2Data data{plug.lv2_host_context, p.effect};
    REQUIRE(data.cv_ports.size() == 1);
    REQUIRE(data.cv_out_ports.size() == 1);
    REQUIRE(data.control_in_ports.size() == 2);

    ossia::float_vector lo, hi, def;
    data.portRanges(lo, hi, def);
    CHECK(lo[1] == 0.f);
    CHECK(hi[1] == 1.f);
    CHECK(def[1] == 0.f);
    CHECK(def[3] == 0.5f);

    SECTION("model")
    {
      QObject parent;
      auto* model = new LV2::Model{
          TimeVal::fromMsecs(1000), "urn:score:test:envelope",
          Id<Process::ProcessModel>{1}, &parent};
      REQUIRE(model->plugin);
      // Gate, Time, Level
      REQUIRE(model->inlets().size() == 3);
      CHECK(model->inlets()[0]->name() == "Gate");
      CHECK(qobject_cast<Process::AudioInlet*>(model->inlets()[0]));
      REQUIRE(model->outlets().size() == 1);
      CHECK(model->outlets()[0]->name() == "Envelope Out");
      CHECK(qobject_cast<Process::AudioOutlet*>(model->outlets()[0]));

      auto* time = dynamic_cast<Process::FloatSlider*>(model->inlets()[1]);
      REQUIRE(time);
      CHECK(time->name() == "Time");
      CHECK(ossia::convert<float>(time->value()) == 0.f);
      CHECK(time->getMax() == 1.f);
      // The inlets after the CV output keep the ids they had when it was one
      CHECK(model->inlets()[1]->id() == Id<Process::Port>{2});
      CHECK(model->inlets()[2]->id() == Id<Process::Port>{3});

      SECTION("a document saved when the CV output was an inlet")
      {
        dynamic_cast<Process::ControlInlet*>(model->inlets()[1])->setValue(0.75f);
        dynamic_cast<Process::ControlInlet*>(model->inlets()[2])->setValue(0.25f);

        JSONReader reader;
        reader.readFrom(static_cast<const Process::ProcessModel&>(*model));
        auto doc = toValue(reader);

        // Put the old "Envelope Out" inlet (id 1) back between Gate and Time
        REQUIRE(doc.HasMember("Inlets"));
        auto& inlets = doc["Inlets"];
        REQUIRE(inlets.Size() == 3);
        rapidjson::Value old_out{inlets[0], doc.GetAllocator()};
        old_out["id"].SetInt(1);
        rapidjson::Value arr{rapidjson::kArrayType};
        arr.PushBack(rapidjson::Value{inlets[0], doc.GetAllocator()}, doc.GetAllocator());
        arr.PushBack(old_out, doc.GetAllocator());
        arr.PushBack(rapidjson::Value{inlets[1], doc.GetAllocator()}, doc.GetAllocator());
        arr.PushBack(rapidjson::Value{inlets[2], doc.GetAllocator()}, doc.GetAllocator());
        inlets = arr;

        JSONObject::Deserializer des{doc};
        auto* loaded = new LV2::Model{des, &parent};
        REQUIRE(loaded->inlets().size() == 3);
        CHECK(
            ossia::convert<float>(
                dynamic_cast<Process::ControlInlet*>(loaded->inlets()[1])->value())
            == Catch::Approx(0.75));
        CHECK(
            ossia::convert<float>(
                dynamic_cast<Process::ControlInlet*>(loaded->inlets()[2])->value())
            == Catch::Approx(0.25));
      }
    }

    SECTION("node")
    {
      ossia::execution_state st;
      {
        test_node node{data, 48000, {LV2::voice_routing::single, 1}, {}, {}};
        // gate, time, level
        REQUIRE(node.root_inputs().size() == 3);
        REQUIRE(node.root_outputs().size() == 1);
        CHECK(node.voices[0]->fInControls[0] == 0.f);

        auto& gate = node.root_inputs()[0]->cast<ossia::audio_port>();
        gate.set_channels(1);
        gate.channel(0).assign(64, 1.);
        node.root_inputs()[1]->cast<ossia::value_port>().write_value(0.5f, 0);
        run_node(node, st);

        auto& out = node.root_outputs()[0]->cast<ossia::audio_port>();
        REQUIRE(out.channels() == 1);
        REQUIRE(out.channel(0).size() == 64);
        CHECK(out.channel(0)[0] == Catch::Approx(1.5));
        CHECK(out.channel(0)[63] == Catch::Approx(1.5));
      }
      QCoreApplication::processEvents();
    }
  });
}

TEST_CASE("an LV2 preset keeps its control values, through a file or a drag", "[lv2][presets]")
{
  prepare_lv2_test_environment();
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    setupLV2(ctx);

    QObject parent;
    auto* model = new LV2::Model{
        TimeVal::fromMsecs(1000), gain_name, Id<Process::ProcessModel>{1}, &parent};
    REQUIRE(model->plugin);
    REQUIRE(model->effectContext.instance);
    auto* gain = dynamic_cast<Process::ControlInlet*>(model->inlets()[1]);
    REQUIRE(gain);

    gain->setValue(0.25f);
    const auto saved = model->savePreset();

    // Its data is JSON: a preset file or a drag carries it whole
    const auto read = Process::Preset::fromJson(
        ctx.interfaces<Process::ProcessFactoryList>(), saved.toJson());
    REQUIRE(read);
    rapidjson::Document doc;
    doc.Parse(read->data.constData(), read->data.size());
    REQUIRE(doc.IsObject());
    REQUIRE(doc.HasMember("State"));

    // The value it saved comes back, set on the control by its port symbol
    gain->setValue(0.75f);
    model->loadPreset(*read);
    CHECK(ossia::convert<float>(gain->value()) == Catch::Approx(0.25f));

    // A preset from before, its data the Turtle itself, still loads
    gain->setValue(0.75f);
    Process::Preset legacy = *read;
    legacy.data = QByteArray{doc["State"].GetString()};
    model->loadPreset(legacy);
    CHECK(ossia::convert<float>(gain->value()) == Catch::Approx(0.25f));
  });
}

// lv2:sampleRate: the bounds of a port are fractions of the sample rate. The
// control keeps the fraction, the plug-in, its state and its UI get Hz.
TEST_CASE("LV2 sample rate ports are given in Hz to the plug-in", "[lv2]")
{
  prepare_lv2_test_environment();
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto& plug = setupLV2(ctx);
    fixture_plugin p{plug, "urn:score:test:rate", "Score Test Rate"};
    LV2::LV2Data data{plug.lv2_host_context, p.effect};
    // rate, gain; rate_echo, gain_echo
    REQUIRE(data.control_in_ports.size() == 2);
    REQUIRE(data.control_out_ports.size() == 2);
    CHECK(data.isSampleRatePort(0));
    CHECK(!data.isSampleRatePort(1));
    CHECK(data.isSampleRatePort(3));
    CHECK(!data.isSampleRatePort(4));

    SECTION("node")
    {
      ossia::execution_state st;
      {
        test_node node{data, 48000, {LV2::voice_routing::single, 1}, {}, {}};
        REQUIRE(node.root_inputs().size() == 2);
        REQUIRE(node.root_outputs().size() == 3);
        auto& out = node.root_outputs()[0]->cast<ossia::audio_port>();
        auto& rate_echo = node.root_outputs()[1]->cast<ossia::value_port>();
        auto& gain_echo = node.root_outputs()[2]->cast<ossia::value_port>();
        auto last = [](ossia::value_port& p) {
          REQUIRE(!p.get_data().empty());
          return ossia::convert<float>(p.get_data().back().value);
        };

        // The default, 1, is the sample rate
        run_node(node, st);
        REQUIRE(out.channels() == 1);
        REQUIRE(out.channel(0).size() == 64);
        CHECK(out.channel(0)[0] == Catch::Approx(48000.));
        CHECK(last(rate_echo) == Catch::Approx(1.f));
        CHECK(last(gain_echo) == Catch::Approx(0.5f));

        rate_echo.get_data().clear();
        gain_echo.get_data().clear();
        node.root_inputs()[0]->cast<ossia::value_port>().write_value(0.25f, 0);
        node.root_inputs()[1]->cast<ossia::value_port>().write_value(0.3f, 0);
        run_node(node, st);
        CHECK(out.channel(0)[63] == Catch::Approx(12000.));
        CHECK(last(rate_echo) == Catch::Approx(0.25f));
        CHECK(last(gain_echo) == Catch::Approx(0.3f));
      }
      QCoreApplication::processEvents();
    }

    SECTION("model")
    {
      QObject parent;
      auto* model = new LV2::Model{
          TimeVal::fromMsecs(1000), "urn:score:test:rate",
          Id<Process::ProcessModel>{1}, &parent};
      REQUIRE(model->plugin);
      REQUIRE(model->effectContext.instance);
      REQUIRE(model->inlets().size() == 2);
      auto* rate = dynamic_cast<Process::FloatSlider*>(model->inlets()[0]);
      auto* gain = dynamic_cast<Process::FloatSlider*>(model->inlets()[1]);
      REQUIRE(rate);
      REQUIRE(gain);
      CHECK(rate->getMin() == 0.f);
      CHECK(rate->getMax() == 1.f);
      CHECK(ossia::convert<float>(rate->value()) == 1.f);

      const float sr = plug.lv2_context->sampleRate;
      CHECK(model->toPlugin(0, 0.5f) == Catch::Approx(sr / 2));

      // The rate follows the one the device runs at
      {
        auto& audio = ctx.settings<Audio::Settings::Model>();
        const int before = audio.getRate();
        const int other = before == 96000 ? 88200 : 96000;
        audio.adoptRate(other);
        CHECK(model->toPlugin(0, 0.5f) == Catch::Approx(other / 2.));
        CHECK(model->fromPlugin(0, other / 4.f) == Catch::Approx(0.25f));
        audio.adoptRate(before);
        CHECK(model->toPlugin(0, 0.5f) == Catch::Approx(sr / 2));
      }
      CHECK(model->fromPlugin(0, sr / 4) == Catch::Approx(0.25f));
      CHECK(model->toPlugin(1, 0.5f) == 0.5f);
      CHECK(model->fromPlugin(1, 0.5f) == 0.5f);
      CHECK(model->toPlugin(3, 0.5f) == Catch::Approx(sr / 2));
      CHECK(model->fromPlugin(4, 0.5f) == 0.5f);

      SECTION("a preset holds Hz and comes back as the fraction")
      {
        rate->setValue(0.25f);
        gain->setValue(0.3f);
        const auto saved = model->savePreset();
        rapidjson::Document doc;
        doc.Parse(saved.data.constData(), saved.data.size());
        REQUIRE(doc.HasMember("State"));

        auto& map = plug.lv2_context->map;
        LilvState* state = lilv_state_new_from_string(
            plug.lilv.me, &map, doc["State"].GetString());
        REQUIRE(state);
        std::map<std::string, float> values;
        lilv_state_emit_port_values(
            state,
            [](const char* sym, void* ud, const void* v, uint32_t, uint32_t) {
          (*static_cast<std::map<std::string, float>*>(ud))[sym]
              = *static_cast<const float*>(v);
            },
            &values);
        lilv_state_free(state);
        CHECK(values["rate"] == Catch::Approx(sr / 4));
        CHECK(values["gain"] == Catch::Approx(0.3f));

        rate->setValue(0.75f);
        gain->setValue(0.75f);
        model->loadPreset(saved);
        CHECK(ossia::convert<float>(rate->value()) == Catch::Approx(0.25f));
        CHECK(ossia::convert<float>(gain->value()) == Catch::Approx(0.3f));
      }
    }
  });
}

// A plug-in's presets come in the order of their URIs, as banks and slots are
// numbered: lilv gives them in an order that changes from run to run.
// Presets are referred to by URI, so the order does not change what loads.
TEST_CASE("LV2 presets are listed in the natural order of their URIs", "[lv2][presets]")
{
  prepare_lv2_test_environment();
  QTemporaryDir dir;
  REQUIRE(dir.isValid());

  // Written out of order; lexicographically preset10 to preset12 would come
  // before preset2
  constexpr auto gain2_uri = "urn:score:test:gain2";
  const std::vector<int> written{7, 10, 2, 12, 1, 9, 11, 3, 5, 8, 4, 6};
  {
    QFile f{dir.filePath("manifest.ttl")};
    REQUIRE(f.open(QIODevice::WriteOnly));
    QByteArray ttl
        = "@prefix lv2: <http://lv2plug.in/ns/lv2core#> .\n"
          "@prefix pset: <http://lv2plug.in/ns/ext/presets#> .\n"
          "@prefix rdfs: <http://www.w3.org/2000/01/rdf-schema#> .\n";
    for(int i : written)
      ttl += QStringLiteral(
                 "<urn:score:test:gain2:preset%1> a pset:Preset ;\n"
                 "  lv2:appliesTo <urn:score:test:gain2> ;\n"
                 "  rdfs:label \"Slot %1\" ;\n"
                 "  lv2:port [ lv2:symbol \"gain\" ; pset:value %2 ] .\n")
                 .arg(i)
                 .arg(i / 4.)
                 .toUtf8();
    f.write(ttl);
  }

  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto& plug = ctx.applicationPlugin<LV2::ApplicationPlugin>();
    plug.lv2_context->loadPlugins();
    auto info = makeGainInfo();
    info.uri = gain2_uri;
    info.preset_bundles = {QUrl::fromLocalFile(dir.path()).toString() + "/"};
    plug.setCachedDescriptors({makeGainInfo(), info});

    QObject parent;
    auto* model = new LV2::Model{
        TimeVal::fromMsecs(1000), gain2_uri, Id<Process::ProcessModel>{1}, &parent};
    REQUIRE(model->plugin);
    REQUIRE(model->effectContext.instance);
    auto* gain = dynamic_cast<Process::ControlInlet*>(model->inlets()[1]);
    REQUIRE(gain);

    const auto uriOf = [](const Process::Preset& p) {
      rapidjson::Document doc;
      doc.Parse(p.data.constData(), p.data.size());
      REQUIRE(doc.IsObject());
      REQUIRE(doc.HasMember("Uri"));
      return QString::fromUtf8(doc["Uri"].GetString());
    };

    const auto presets = model->builtinPresets();
    REQUIRE(presets.size() == 12);
    for(int i = 0; i < 12; i++)
    {
      INFO(i);
      CHECK(presets[i].name == QStringLiteral("Slot %1").arg(i + 1));
      CHECK(uriOf(presets[i]) == QStringLiteral("urn:score:test:gain2:preset%1").arg(i + 1));
    }

    // The same every time it is asked
    const auto again = model->builtinPresets();
    REQUIRE(again.size() == presets.size());
    for(std::size_t i = 0; i < presets.size(); i++)
      CHECK(again[i].data == presets[i].data);

    SECTION("a preset saved before still loads the same one")
    {
      const QByteArray uri = "urn:score:test:gain2:preset10";

      // In a preset file or a drag: {"Uri": ...}
      Process::Preset saved = presets[9];
      gain->setValue(0.f);
      const auto read = Process::Preset::fromJson(
          ctx.interfaces<Process::ProcessFactoryList>(), saved.toJson());
      REQUIRE(read);
      CHECK(uriOf(*read) == QString::fromUtf8(uri));
      model->loadPreset(*read);
      CHECK(ossia::convert<float>(gain->value()) == Catch::Approx(10 / 4.));

      // From before the JSON data: the URI itself
      gain->setValue(0.f);
      Process::Preset legacy = saved;
      legacy.data = uri;
      model->loadPreset(legacy);
      CHECK(ossia::convert<float>(gain->value()) == Catch::Approx(10 / 4.));
    }
  });
}

// An event monitor forwards what it receives, positions included: its output
// says it may carry one, and stays an output. Its input is typed as a
// sequence. The run telling a stop lasts a tick, not one frame. Between two
// plays its ports point at nothing that is gone.
TEST_CASE("LV2 event ports: sequences, outputs supporting time:Position, a stop runs a tick", "[lv2]")
{
  prepare_lv2_test_environment();
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto& plug = setupLV2(ctx);
    fixture_plugin p{plug, "urn:score:test:thru", "Score Test Thru", true};
    LV2::LV2Data data{plug.lv2_host_context, p.effect};
    REQUIRE(data.midi_in_ports.size() == 1);
    REQUIRE(data.midi_out_ports.size() == 1);
    REQUIRE(data.time_Position_ports.size() == 1);
    CHECK(data.time_Position_ports[0] == 0);

    ossia::execution_state st;
    const void* gone{};
    {
      test_node node{data, 48000, {LV2::voice_routing::single, 1}, {}, {}};
      auto& in = node.root_inputs()[0]->cast<ossia::midi_port>();
      const uint64_t on = cmidi2_ump_midi2_note_on(0, 0, 60, 0, 100 << 9, 0);
      libremidi::ump u{uint32_t(on >> 32), uint32_t(on & 0xffffffff)};
      u.timestamp = 3;
      in.messages.push_back(u);
      run_node(node, st);

      auto& out = node.root_outputs()[0]->cast<ossia::midi_port>();
      std::vector<int> notes;
      for(auto& m : out.messages)
        if(((m.data[0] >> 20) & 0xF) == 0x9)
          notes.push_back(int((m.data[0] >> 8) & 0x7F));
      CHECK(notes == std::vector<int>{60});
      // What the plug-in is given is an atom:Sequence, whatever it holds
      CHECK(node.voices[0]->fOutControls[1] == 1.f);

      node.mustStop = true;
      run_node(node, st, false);
      CHECK(node.voices[0]->fOutControls[0] == 64.f);
      gone = &node.voices[0]->fOutControls[0];
    }
    QCoreApplication::processEvents();

#if defined(__linux__)
    // The instance lives on to the next play: the buffers of the play that
    // ended are not written by the ports connected for this one
    {
      const auto so = QString::fromUtf8(SCORE_TEST_LV2_BUNDLE) + "/score_test_gain.so";
      void* lib = dlopen(so.toUtf8().constData(), RTLD_NOW | RTLD_NOLOAD);
      REQUIRE(lib);
      auto first_write = reinterpret_cast<const void* (*)()>(
          dlsym(lib, "score_test_thru_first_write"));
      auto forget = reinterpret_cast<void (*)()>(dlsym(lib, "score_test_thru_forget"));
      REQUIRE(first_write);
      REQUIRE(forget);
      forget();
      test_node node{data, 48000, {LV2::voice_routing::single, 1}, {}, {}};
      REQUIRE(first_write());
      CHECK(first_write() != gone);
      dlclose(lib);
    }
    QCoreApplication::processEvents();
#endif
  });
}

#if defined(__linux__)
// Two plays of one instance overlap: the voices of the one that ended go
// after the next one took the instance. They leave it active, and every port
// of the instance, those with no direction included, is run with the buffers
// of the voice that plays, never with the one ports are parked on.
TEST_CASE("LV2 instance handed over: it stays active, no port runs parked", "[lv2]")
{
  prepare_lv2_test_environment();
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto& plug = setupLV2(ctx);
    fixture_plugin p{plug, "urn:score:test:handover", "Score Test Handover"};
    LV2::LV2Data data{plug.lv2_host_context, p.effect};
    REQUIRE(data.control_out_ports.size() == 1);
    REQUIRE(data.midi_other_ports.size() == 1);
    REQUIRE(data.atom_other_ports.size() == 2);

    const auto so = QString::fromUtf8(SCORE_TEST_LV2_BUNDLE) + "/score_test_gain.so";
    void* lib = dlopen(so.toUtf8().constData(), RTLD_NOW | RTLD_NOLOAD);
    REQUIRE(lib);
    auto connected = reinterpret_cast<const void* (*)(uint32_t)>(
        dlsym(lib, "score_test_handover_connected"));
    auto ran_with = reinterpret_cast<const void* (*)(uint32_t)>(
        dlsym(lib, "score_test_handover_ran_with"));
    REQUIRE(connected);
    REQUIRE(ran_with);

    ossia::execution_state st;
    auto active = [](test_node& n) { return n.voices[0]->fOutControls[0]; };

    auto first = std::make_unique<test_node>(
        data, 48000, LV2::voice_strategy{LV2::voice_routing::single, 1},
        noop_hook{}, noop_hook{});
    run_node(*first, st);
    CHECK(active(*first) == 1.f);
    for(uint32_t port : {1u, 2u})
    {
      INFO("port " << port);
      CHECK(ran_with(port) == &first->voices[0]->atom_others[port - 1].buf->atoms);
    }

    // The next play takes the instance before the voices of this one go
    auto second = std::make_unique<test_node>(
        data, 48000, LV2::voice_strategy{LV2::voice_routing::single, 1},
        noop_hook{}, noop_hook{});
    REQUIRE(second->voices[0]->instance == first->voices[0]->instance);
    first.reset();
    QCoreApplication::processEvents();
    CHECK(p.effect.instance_holder->activated);
    run_node(*second, st);
    CHECK(active(*second) == 1.f);
    for(uint32_t port : {1u, 2u})
    {
      INFO("port " << port);
      CHECK(ran_with(port) == &second->voices[0]->atom_others[port - 1].buf->atoms);
    }

    // Its voices go: the instance is parked until the next play takes it
    second.reset();
    QCoreApplication::processEvents();
    const void* parked = connected(1);
    REQUIRE(parked);
    {
      test_node third{data, 48000, {LV2::voice_routing::single, 1}, {}, {}};
      run_node(third, st);
      CHECK(active(third) == 1.f);
      for(uint32_t port : {0u, 1u, 2u})
      {
        INFO("port " << port);
        CHECK(ran_with(port) != parked);
      }
    }
    QCoreApplication::processEvents();
    dlclose(lib);
  });
}
#endif
