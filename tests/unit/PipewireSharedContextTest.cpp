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

  pw_stream* stream{};
  void* badProxy{};
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
  }
  pw.thread_loop_unlock(loopBefore);
  REQUIRE(stream != nullptr);

  // Give the daemon time to deliver the error and the context time to act on
  // it. The assertion below is that it does NOT act on it: a per-object error
  // says nothing about the socket, and treating it as connection loss is what
  // used to start the teardown this case is named after.
  for(int i = 0; i < 25; i++)
    std::this_thread::sleep_for(20ms);

  REQUIRE(holderA->state() != libremidi::pipewire::connection_state::broken);
  CHECK(holderA->state() == libremidi::pipewire::connection_state::connected);

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
    pw.proxy_destroy(static_cast<pw_proxy*>(badProxy));
  pw.stream_destroy(stream);
  pw.thread_loop_unlock(loopBefore);

  SUCCEED("pw_stream_destroy survived the peer device start");
}
