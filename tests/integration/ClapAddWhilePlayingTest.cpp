// CLAP plug-ins added, edited and removed while the document plays, in every
// way of score_test/LivePlugin.hpp: the fixture effects (clap-mono-gain), one
// that score runs as an instance per channel and one it runs as it is, count
// what the host does that the specification forbids, and the editor fixture
// (clap-sized-editor) has no audio port.

#include <score_test/App.hpp>
#include <score_test/LivePlugin.hpp>

#include <catch2/catch_test_macros.hpp>

#include <dlfcn.h>

#include <string>
#include <vector>

namespace
{
const auto clap_key
    = UuidKey<Process::ProcessModel>::fromString(QStringLiteral("10e8d26d-1d82-4bfc-a9fb-d85ffdf04e5f"));

struct errors
{
  int wrong_thread;
  int gestures;
  int foreign_cookies;
  int bad_events;
  int bad_state;
  int late_calls;
  int double_init;
  int most_params;
  int activations;
  int instances;
  int active;
  int initialised;
  double last_rate;
};

//! What clap-mono-gain counted since the last call
errors report()
{
  void* lib = dlopen(CLAP_MONO_GAIN_PATH, RTLD_LAZY);
  REQUIRE(lib);
  auto f = reinterpret_cast<errors (*)()>(dlsym(lib, "clap_mono_gain_report"));
  REQUIRE(f);
  return f();
}

score::test::live::options options_for(const char* path, const char* id)
{
  score::test::live::options opt;
  opt.what = {clap_key, {}, QString::fromUtf8(path) + ":::" + id};
  return opt;
}

//! Every phase runs or is skipped, none fails
void all_phases_hold(
    const score::GUIApplicationContext& ctx, const score::test::live::options& opt)
{
  std::vector<std::string> failures;
  int ok = 0;
  score::test::live::recorder rec;
  rec.result = [&](std::string_view phase, std::string_view verdict, std::string_view detail) {
    if(verdict == "fail")
      failures.push_back(std::string{phase} + ": " + std::string{detail});
    else if(verdict == "ok")
      ok++;
  };
  score::test::live::run(ctx, opt, rec);
  for(auto& f : failures)
    UNSCOPED_INFO(f);
  CHECK(failures.empty());
  CHECK(ok > 0);
}

void no_host_error(const errors& e)
{
  CHECK(e.wrong_thread == 0);
  CHECK(e.bad_events == 0);
  CHECK(e.bad_state == 0);
  CHECK(e.late_calls == 0);
  CHECK(e.double_init == 0);
  CHECK(e.active == 0);
  CHECK(e.instances == 0);
}
}

TEST_CASE(
    "CLAP effects added, edited and removed while playing, in every way",
    "[integration][execution][clap]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    report();
    for(auto id : {"io.ossia.test.stereo-gain", "io.ossia.test.mono-gain"})
    {
      INFO(id);
      all_phases_hold(ctx, options_for(CLAP_MONO_GAIN_PATH, id));
      score::test::live::spin(100);
      no_host_error(report());
    }
  });
}

// Removing a process does not stop its interval: the plug-in has to stop
// processing, on the audio thread, before the main thread deactivates it.
TEST_CASE(
    "a CLAP plug-in removed while playing stops processing before it is deactivated",
    "[integration][execution][clap]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    report();
    for(auto id : {"io.ossia.test.stereo-gain", "io.ossia.test.mono-gain"})
    {
      INFO(id);
      auto opt = options_for(CLAP_MONO_GAIN_PATH, id);
      opt.only = {"live-remove-undo-redo", "live-undo-redo-add", "live-replace"};
      all_phases_hold(ctx, opt);
      score::test::live::spin(100);
      const auto e = report();
      CHECK(e.bad_state == 0);
      CHECK(e.wrong_thread == 0);
      CHECK(e.active == 0);
    }
  });
}

TEST_CASE(
    "a CLAP plug-in with an editor and no audio port added while playing",
    "[integration][execution][clap]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    all_phases_hold(ctx, options_for(CLAP_SIZED_EDITOR_PATH, "io.ossia.test.resizable-editor"));
  });
}
