// Reproduces the teardown SIGSEGV of PipewireRoundtrip_s2s against a real
// PipeWire daemon spawned in a scratch PIPEWIRE_RUNTIME_DIR.
//
// The process-wide libremidi::pipewire::context is shared by the gfx output
// producer, the gfx input consumer, the audio engine and the MIDI backends.
// An -ENOENT reply from the daemon used to flip the whole connection to
// connection_state::broken, including the per-object error a stream gets when
// its autoconnect finds no target node. A device that then calls reconnect()
// on it destroys the pw_core, pw_context and pw_thread_loop; every pw_stream
// another holder created on that core is left pointing at freed memory, and
// the next pw_stream_destroy() faults inside pw_loop_check().
//
// libremidi now reads the id the error carries and only treats PW_ID_CORE as
// connection loss, so the misclassification that started the teardown cannot
// happen. This case asserts both halves: the per-object error leaves the
// connection alone, and the context another holder picks up afterwards is
// still the same loop and core.
//
// Skips when no daemon can be started (no pipewire binary, no libpipewire).

#include <Gfx/Pipewire/PipewireSharedContext.hpp>

#include <libremidi/backends/linux/pipewire/loader.hpp>

#include <catch2/catch_test_macros.hpp>

#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <string>
#include <thread>

namespace
{
using namespace std::literals;

struct daemon_fixture
{
  std::string dir;
  pid_t pid{-1};

  bool start()
  {
    char tmpl[] = "/tmp/score-pw-shared-ctx-XXXXXX";
    if(!mkdtemp(tmpl))
      return false;
    dir = tmpl;

    setenv("PIPEWIRE_RUNTIME_DIR", dir.c_str(), 1);
    unsetenv("PIPEWIRE_REMOTE");

    pid = fork();
    if(pid == 0)
    {
      if(int devnull = open("/dev/null", O_RDWR); devnull >= 0)
      {
        dup2(devnull, STDOUT_FILENO);
        dup2(devnull, STDERR_FILENO);
      }
      execlp("pipewire", "pipewire", (char*)nullptr);
      _exit(127);
    }
    if(pid < 0)
      return false;

    const std::string sock = dir + "/pipewire-0";
    for(int i = 0; i < 100; i++)
    {
      if(access(sock.c_str(), F_OK) == 0)
        return true;
      int status{};
      if(waitpid(pid, &status, WNOHANG) == pid)
      {
        pid = -1;
        return false;
      }
      std::this_thread::sleep_for(50ms);
    }
    return false;
  }

  ~daemon_fixture()
  {
    if(pid > 0)
    {
      kill(pid, SIGTERM);
      int status{};
      waitpid(pid, &status, 0);
    }
    if(!dir.empty())
      std::system(("rm -rf " + dir + " >/dev/null 2>&1").c_str());
  }
};
}

TEST_CASE(
    "shared pipewire context survives a peer device start",
    "[pipewire][gfx]")
{
  auto& pw = libremidi::pipewire::load();
  if(!pw.stream_available || !pw.thread_available)
    SKIP("libpipewire-0.3 is not available at runtime");

  daemon_fixture daemon;
  if(!daemon.start())
    SKIP("could not start a private pipewire daemon");

  // Holder A: stands in for the gfx output producer. Owns a live pw_stream
  // built on the shared connection's core and thread loop.
  auto holderA = libremidi::pipewire::shared_context();
  REQUIRE(holderA);
  REQUIRE(holderA->ok());

  auto* const loopBefore = holderA->thread_loop_handle();
  auto* const coreBefore = holderA->pw_core_ptr();
  REQUIRE(loopBefore != nullptr);
  REQUIRE(coreBefore != nullptr);

  // The error the daemon sends back for the bad request below. The test waits
  // for this rather than for a wall-clock interval: without proof that the
  // error arrived, asserting that it did not break the connection proves
  // nothing at all.
  std::atomic_int perObjectErrors{0};
  static std::atomic_int* s_errors{};
  s_errors = &perObjectErrors;

  static constexpr pw_proxy_events proxyEvents = [] {
    pw_proxy_events e{};
    e.version = PW_VERSION_PROXY_EVENTS;
    e.error = [](void*, int, int res, const char*) {
      if(res != 0 && s_errors)
        s_errors->fetch_add(1, std::memory_order_release);
    };
    return e;
  }();

  pw_stream* stream{};
  void* badProxy{};
  spa_hook proxyHook{};
  pw.thread_loop_lock(loopBefore);
  {
    auto* props = pw.properties_new(
        PW_KEY_MEDIA_TYPE, "Video", PW_KEY_MEDIA_CATEGORY, "Source", nullptr);
    stream = pw.stream_new(coreBefore, "score-shared-context-test", props);
    if(stream)
      pw.stream_connect(
          stream, PW_DIRECTION_OUTPUT, PW_ID_ANY, PW_STREAM_FLAG_MAP_BUFFERS,
          nullptr, 0);

    // An -ENOENT reply on a per-object request, which is what a stream whose
    // autoconnect finds no target node gets from the daemon.
    badProxy = pw_core_create_object(
        coreBefore, "score-no-such-factory", PW_TYPE_INTERFACE_Node,
        PW_VERSION_NODE, nullptr, 0);
    if(badProxy && pw.proxy_add_listener)
      pw.proxy_add_listener(
          static_cast<pw_proxy*>(badProxy), &proxyHook, &proxyEvents, nullptr);
  }
  pw.thread_loop_unlock(loopBefore);
  REQUIRE(stream != nullptr);
  REQUIRE(badProxy != nullptr);

  // A core round-trip, not a sleep. The daemon answers in order, so once this
  // returns, the reply to the create_object above has been delivered and the
  // context has already classified it.
  //
  // Its return value is the assertion. synchronize() is false exactly when the
  // connection has been marked broken, which is what a per-object error used
  // to do -- and a device reacting to `broken` by reconnecting is what tore
  // the loop out from under the other holders. The id the error carries says
  // it belongs to one object, not to the socket.
  for(int i = 0; i < 50 && perObjectErrors.load(std::memory_order_acquire) == 0; i++)
    REQUIRE(holderA->synchronize());

  // The premise: the daemon really did refuse the request.
  REQUIRE(perObjectErrors.load(std::memory_order_acquire) > 0);

  // The property: refusing it left the connection alone.
  CHECK(holderA->ok());
  CHECK(holderA->state() == libremidi::pipewire::connection_state::connected);
  CHECK(holderA->synchronize());

  // Holder B: stands in for the gfx input device starting up afterwards.
  auto holderB = Gfx::PipeWire::acquireSharedContext("regression test");
  REQUIRE(holderB);

  // REQUIRE, not CHECK: past this point the test touches the loop again, and
  // a rebuilt connection means the old one has already been freed.
  REQUIRE(holderB->thread_loop_handle() == loopBefore);
  REQUIRE(holderB->pw_core_ptr() == coreBefore);

  // Holder A's stream must still be destroyable: this is the call that
  // faulted inside pw_loop_check() once the loop had been rebuilt.
  pw.thread_loop_lock(loopBefore);
  if(badProxy && pw.proxy_destroy)
  {
    spa_hook_remove(&proxyHook);
    pw.proxy_destroy(static_cast<pw_proxy*>(badProxy));
  }
  s_errors = nullptr;
  pw.stream_destroy(stream);
  pw.thread_loop_unlock(loopBefore);

  SUCCEED("pw_stream_destroy survived the peer device start");
}
