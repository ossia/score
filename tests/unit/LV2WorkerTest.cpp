// The worker of an LV2 plug-in, as the audio node runs it: what a run
// schedules is done on the instance that asked and answered to it, also when
// two nodes run at once on two threads (the parallel scheduler); an answer may
// schedule a work in its turn; and the audio thread allocates and frees
// nothing for it. Against the hermetic fixture bundle (tests/fixtures/lv2):
// urn:score:test:worker-echo schedules a work at each run.

#include "AllocCounter.hpp"

#include <LV2/ApplicationPlugin.hpp>
#include <LV2/Context.hpp>
#include <LV2/EffectModel.hpp>
#include <LV2/Node.hpp>

#include <Media/AudioPluginCache.hpp>

#include <score_test/App.hpp>

#include <ossia/dataflow/exec_state_facade.hpp>
#include <ossia/dataflow/execution_state.hpp>

#include <QCoreApplication>

#include <catch2/catch_test_macros.hpp>

#include <dlfcn.h>

#include <atomic>
#include <chrono>
#include <thread>

namespace
{
constexpr auto echo_uri = "urn:score:test:worker-echo";

QString bundlePath()
{
  return QString::fromUtf8(SCORE_TEST_LV2_BUNDLE);
}

struct noop_hook
{
  void operator()() const noexcept { }
};
using test_node = LV2::lv2_node<noop_hook, noop_hook>;

struct echo_plugin
{
  LV2::EffectContext effect;
  explicit echo_plugin(LV2::ApplicationPlugin& plug)
  {
    auto res = LV2::find_lv2_plugin(plug.lilv, echo_uri);
    REQUIRE(res);
    effect.plugin = *res;
    effect.instance_holder = plug.lv2_context->instantiate(effect.plugin.me, 48000);
    REQUIRE(effect.instance_holder);
    effect.instance = effect.instance_holder->instance;
  }
};

struct counters
{
  void* lib{};
  using counter = int (*)();
  counter scheduled{}, schedule_unknown{}, schedule_no_space{}, works{}, misrouted{},
      responses{}, response_scheduled{}, response_schedule_failed{};
  void (*reset)(){};

  counters()
  {
    const auto path = (bundlePath() + "/score_test_gain.so").toUtf8();
    lib = dlopen(path.constData(), RTLD_NOW);
    REQUIRE(lib);
    auto get = [&](const char* n) {
      auto f = reinterpret_cast<counter>(dlsym(lib, n));
      REQUIRE(f);
      return f;
    };
    scheduled = get("score_test_echo_scheduled");
    schedule_unknown = get("score_test_echo_schedule_unknown");
    schedule_no_space = get("score_test_echo_schedule_no_space");
    works = get("score_test_echo_works");
    misrouted = get("score_test_echo_misrouted");
    responses = get("score_test_echo_responses");
    response_scheduled = get("score_test_echo_response_scheduled");
    response_schedule_failed = get("score_test_echo_response_schedule_failed");
    reset = reinterpret_cast<void (*)()>(dlsym(lib, "score_test_echo_reset"));
    REQUIRE(reset);
    reset();
  }
  ~counters() { dlclose(lib); }
};

struct runner
{
  test_node node;
  ossia::execution_state st;

  runner(LV2::LV2Data& data)
      : node{data, 48000, {LV2::voice_routing::single, 1}, {}, {}}
  {
    st.bufferSize = 64;
    auto& in = node.root_inputs()[0]->cast<ossia::audio_port>();
    in.set_channels(1);
    in.channel(0).resize(64);
  }

  void tick()
  {
    ossia::exec_state_facade fac{&st};
    ossia::token_request tk{
        ossia::time_value{0},       ossia::time_value{64},
        ossia::time_value{1000000}, ossia::time_value{0},
        1.,                         ossia::time_signature{4, 4},
        120.};
    tk.start_sample = 0;
    tk.length_sample = 64;
    static_cast<ossia::graph_node&>(node).run(tk, fac);
  }
};

void wait_quiet(const counters& c)
{
  using namespace std::chrono;
  auto last = c.works();
  for(int i = 0; i < 50; i++)
  {
    std::this_thread::sleep_for(milliseconds(20));
    const auto now = c.works();
    if(now == last)
      return;
    last = now;
  }
}

void setup(LV2::ApplicationPlugin& plug)
{
  plug.lv2_context->loadPlugins();
  LV2::PluginInfo info;
  info.bundle = bundlePath();
  info.uri = echo_uri;
  info.name = "Score Test Worker Echo";
  info.class_label = "Utility";
  info.valid = true;
  plug.setCachedDescriptors({info});
}
}

TEST_CASE(
    "LV2 works are answered to the instance that asked, with no allocation on the "
    "audio thread",
    "[lv2]")
{
  qputenv("SCORE_DISABLE_AUDIOPLUGINS", "1");
  qputenv("LV2_PATH", bundlePath().toUtf8());
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto& plug = ctx.applicationPlugin<LV2::ApplicationPlugin>();
    setup(plug);
    counters c;
    {
      echo_plugin p{plug};
      LV2::LV2Data data{plug.lv2_host_context, p.effect};
      runner r{data};

      // The first runs size the outputs
      for(int i = 0; i < 4; i++)
        r.tick();
      wait_quiet(c);

      int allocations = 0, frees = 0;
      for(int i = 0; i < 400; i++)
      {
        {
          alloc_counter::scope s;
          r.tick();
          allocations += s.allocations();
          frees += s.frees();
        }
        std::this_thread::sleep_for(std::chrono::microseconds(500));
      }
      wait_quiet(c);
      r.tick();
      wait_quiet(c);
      r.tick();

      INFO("scheduled " << c.scheduled() << ", no space " << c.schedule_no_space());
      CHECK(c.responses() > 200);
      CHECK(c.misrouted() == 0);
      CHECK(c.schedule_unknown() == 0);
      // What an answer schedules is done too
      CHECK(c.response_scheduled() > 100);
      CHECK(c.response_schedule_failed() == 0);
      CHECK(allocations == 0);
      CHECK(frees == 0);
    }
    wait_quiet(c);
    QCoreApplication::processEvents();
  });
}

TEST_CASE("LV2 works of two nodes run on two threads at once stay apart", "[lv2]")
{
  qputenv("SCORE_DISABLE_AUDIOPLUGINS", "1");
  qputenv("LV2_PATH", bundlePath().toUtf8());
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto& plug = ctx.applicationPlugin<LV2::ApplicationPlugin>();
    setup(plug);
    counters c;
    {
      echo_plugin p1{plug}, p2{plug};
      LV2::LV2Data d1{plug.lv2_host_context, p1.effect};
      LV2::LV2Data d2{plug.lv2_host_context, p2.effect};
      runner r1{d1}, r2{d2};
      r1.tick();
      r2.tick();
      wait_quiet(c);
      c.reset();

      std::atomic_bool go{false};
      auto loop = [&](runner& r) {
        while(!go.load())
          ;
        for(int i = 0; i < 5000; i++)
        {
          r.tick();
          if(i % 16 == 0)
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
      };
      std::thread t1{[&] { loop(r1); }};
      std::thread t2{[&] { loop(r2); }};
      go = true;
      t1.join();
      t2.join();
      wait_quiet(c);
      r1.tick();
      r2.tick();
      wait_quiet(c);

      INFO("scheduled " << c.scheduled() << ", no space " << c.schedule_no_space());
      CHECK(c.scheduled() > 1000);
      CHECK(c.works() > 1000);
      CHECK(c.misrouted() == 0);
      CHECK(c.schedule_unknown() == 0);
    }
    wait_quiet(c);
    QCoreApplication::processEvents();
  });
}
