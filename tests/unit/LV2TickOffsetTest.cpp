// An LV2 plug-in run for a tick that covers only part of the audio buffer (a
// process starting mid-buffer, a loop point splitting the buffer in two): its
// audio and CV inputs are read, and its outputs written, at the frames of the
// tick in the buffer -- the first part of a split buffer is not overwritten by
// the second. Against the hermetic fixture bundle (tests/fixtures/lv2): the
// envelope plug-in outputs, as CV, its CV input plus its Time control.

#include <LV2/ApplicationPlugin.hpp>
#include <LV2/Context.hpp>
#include <LV2/EffectModel.hpp>
#include <LV2/Node.hpp>

#include <Media/AudioPluginCache.hpp>

#include <score_test/App.hpp>

#include <ossia/dataflow/exec_state_facade.hpp>
#include <ossia/dataflow/execution_state.hpp>

#include <QCoreApplication>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

namespace
{
constexpr auto envelope_uri = "urn:score:test:envelope";

QString bundlePath()
{
  return QString::fromUtf8(SCORE_TEST_LV2_BUNDLE);
}

struct noop_hook
{
  void operator()() const noexcept { }
  void operator()(auto&) const noexcept { }
};
using test_node = LV2::lv2_node<noop_hook, noop_hook>;

struct envelope_plugin
{
  LV2::EffectContext effect;
  explicit envelope_plugin(LV2::ApplicationPlugin& plug)
  {
    LV2::PluginInfo info;
    info.bundle = bundlePath();
    info.uri = envelope_uri;
    info.name = "Score Test Envelope";
    info.class_label = "Utility";
    info.valid = true;
    plug.setCachedDescriptors({info});
    auto res = LV2::find_lv2_plugin(plug.lilv, envelope_uri);
    REQUIRE(res);
    effect.plugin = *res;
    auto* inst = lilv_plugin_instantiate(
        effect.plugin.me, 48000, plug.lv2_context->features());
    REQUIRE(inst);
    effect.instance_holder = std::make_shared<LV2::InstanceHandle>(inst);
    effect.instance = inst;
  }
};

//! A tick over frames [start; start + length[ of the buffer
void run_span(test_node& node, ossia::execution_state& st, int start, int length)
{
  ossia::exec_state_facade fac{&st};
  ossia::token_request tk{
      ossia::time_value{start},   ossia::time_value{start + length},
      ossia::time_value{1000000}, ossia::time_value{0},
      1.,                         ossia::time_signature{4, 4},
      120.};
  tk.start_sample = start;
  tk.length_sample = length;
  static_cast<ossia::graph_node&>(node).run(tk, fac);
}
}

TEST_CASE("LV2 CV and audio follow the tick's frames in the buffer", "[lv2]")
{
  qputenv("SCORE_DISABLE_AUDIOPLUGINS", "1");
  qputenv("LV2_PATH", bundlePath().toUtf8());
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto& plug = ctx.applicationPlugin<LV2::ApplicationPlugin>();
    plug.lv2_context->loadPlugins();
    envelope_plugin p{plug};
    LV2::LV2Data data{plug.lv2_host_context, p.effect};
    REQUIRE(data.cv_ports.size() == 1);
    REQUIRE(data.cv_out_ports.size() == 1);

    ossia::execution_state st;
    st.bufferSize = 64;
    {
      test_node node{data, 48000, {LV2::voice_routing::single, 1}, {}, {}};
      // gate (CV), time, level
      auto& gate = node.root_inputs()[0]->cast<ossia::audio_port>();
      gate.set_channels(1);
      gate.channel(0).resize(64);
      for(int i = 0; i < 64; i++)
        gate.channel(0)[i] = i;
      node.root_inputs()[1]->cast<ossia::value_port>().write_value(0.5f, 0);
      auto& out = node.root_outputs()[0]->cast<ossia::audio_port>();

      SECTION("a tick starting mid-buffer")
      {
        run_span(node, st, 16, 32);
        REQUIRE(out.channels() == 1);
        REQUIRE(out.channel(0).size() == 64);
        CHECK(out.channel(0)[15] == 0.);
        CHECK(out.channel(0)[16] == Catch::Approx(16.5));
        CHECK(out.channel(0)[47] == Catch::Approx(47.5));
        CHECK(out.channel(0)[48] == 0.);
      }

      SECTION("a buffer split in two ticks keeps both halves")
      {
        run_span(node, st, 0, 20);
        run_span(node, st, 20, 44);
        REQUIRE(out.channel(0).size() == 64);
        for(int i = 0; i < 64; i++)
        {
          INFO("frame " << i);
          CHECK(out.channel(0)[i] == Catch::Approx(i + 0.5));
        }
      }
    }
    QCoreApplication::processEvents();
  });
}
