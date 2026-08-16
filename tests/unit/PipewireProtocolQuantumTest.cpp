// Reproduces the "unexpected block size 512 (expected 128)" failure against
// a real PipeWire daemon: a private daemon is spawned in a scratch
// PIPEWIRE_RUNTIME_DIR and its global clock.force-quantum setting (which
// overrides every node.force-quantum, exactly like a competing client whose
// force has a newer stamp) pins the graph to a quantum the engine did not ask
// for. The old protocol skipped every such cycle — no tick ever ran and the
// outputs stayed in NEED_DATA (silence). The fixed protocol must keep
// ticking, in slices of the configured block size.
//
// Skips when no daemon can be started (no pipewire/pw-metadata binaries,
// no libpipewire at runtime).

#include <ossia/detail/config.hpp>

#include <ossia/audio/pipewire_protocol.hpp>

#include <catch2/catch_test_macros.hpp>

#if !defined(OSSIA_AUDIO_PIPEWIRE)
TEST_CASE("pipewire quantum integration", "[pipewire][integration]")
{
  SKIP("pipewire support is not compiled in");
}
#else

#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
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
    char tmpl[] = "/tmp/ossia-pw-test-XXXXXX";
    if (!mkdtemp(tmpl))
      return false;
    dir = tmpl;

    // Both the daemon below and this process's own libpipewire connection
    // must resolve to the scratch socket, never to the user's session.
    setenv("PIPEWIRE_RUNTIME_DIR", dir.c_str(), 1);
    unsetenv("PIPEWIRE_REMOTE");

    pid = fork();
    if (pid == 0)
    {
      if (int devnull = open("/dev/null", O_RDWR); devnull >= 0)
      {
        dup2(devnull, STDOUT_FILENO);
        dup2(devnull, STDERR_FILENO);
      }
      execlp("pipewire", "pipewire", (char*)nullptr);
      _exit(127);
    }
    if (pid < 0)
      return false;

    const std::string sock = dir + "/pipewire-0";
    for (int i = 0; i < 100; i++)
    {
      if (access(sock.c_str(), F_OK) == 0)
        return true;
      int status{};
      if (waitpid(pid, &status, WNOHANG) == pid)
      {
        pid = -1;
        return false;
      }
      std::this_thread::sleep_for(50ms);
    }
    return false;
  }

  // The global clock.force-quantum setting: overrides node.force-quantum of
  // every client, which is also what losing the force-quantum stamp race
  // against another client looks like from this process.
  bool force_quantum(int q)
  {
    const auto cmd = "pw-metadata -n settings 0 clock.force-quantum "
                     + std::to_string(q) + " >/dev/null 2>&1";
    return std::system(cmd.c_str()) == 0;
  }

  // Same, for the rate: overrides node.force-rate of every client.
  bool force_rate(int r)
  {
    const auto cmd = "pw-metadata -n settings 0 clock.force-rate "
                     + std::to_string(r) + " >/dev/null 2>&1";
    return std::system(cmd.c_str()) == 0;
  }

  void stop()
  {
    if (pid > 0)
    {
      kill(pid, SIGTERM);
      int status{};
      waitpid(pid, &status, 0);
      pid = -1;
    }
    unsetenv("PIPEWIRE_RUNTIME_DIR");
    if (!dir.empty())
    {
      std::error_code ec;
      std::filesystem::remove_all(dir, ec);
      dir.clear();
    }
  }

  ~daemon_fixture() { stop(); }
};

struct tick_recorder
{
  std::array<std::atomic<std::uint32_t>, 8192> sizes{};
  std::atomic<std::uint32_t> count{};

  // Single producer (the RT thread): publish the slot before the count,
  // or the reader can see count == i+1 while sizes[i] is still zero.
  void push(std::uint32_t n)
  {
    const auto i = count.load(std::memory_order_relaxed);
    if (i < sizes.size())
      sizes[i].store(n, std::memory_order_relaxed);
    count.store(i + 1, std::memory_order_release);
  }

  std::uint32_t stored() const
  {
    return std::min<std::uint32_t>(count.load(std::memory_order_acquire), sizes.size());
  }
};

bool wait_for(auto&& predicate, std::chrono::milliseconds timeout)
{
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline)
  {
    if (predicate())
      return true;
    std::this_thread::sleep_for(10ms);
  }
  return predicate();
}
}

TEST_CASE(
    "pipewire: graph quantum diverging from the configured block size",
    "[pipewire][integration]")
{
  daemon_fixture daemon;
  if (!daemon.start())
    SKIP("cannot start a private pipewire daemon");

  // Pin the graph to 512 *before* the engine joins: the engine will ask for
  // 128 and lose — the exact configuration behind the reported
  // "unexpected block size 512 (expected 128), skipping cycle" spam.
  if (!daemon.force_quantum(512))
    SKIP("pw-metadata is unavailable");

  auto inst = libremidi::pipewire::shared_instance();
  if (!inst)
    SKIP("libpipewire is unavailable");
  auto ctx = libremidi::pipewire::context::make(inst);
  if (!ctx || !ctx->ok())
    SKIP("cannot connect to the private pipewire daemon");

  ossia::audio_setup setup;
  setup.name = "ossia-quantum-test";
  setup.inputs = {"in_l", "in_r"};
  setup.outputs = {"out_l", "out_r"};
  setup.rate = 48000;
  setup.buffer_size = 128;

  auto engine = std::make_shared<ossia::pipewire_audio_protocol>(ctx, setup);
  REQUIRE(engine->running());

  auto rec = std::make_shared<tick_recorder>();
  engine->set_tick([rec](const ossia::audio_tick_state& st) {
    rec->push(static_cast<std::uint32_t>(st.frames));
  });

  // A 512-frame cycle must yield 4 ticks of 128 frames; the unfixed
  // protocol never ticks at all here.
  REQUIRE(wait_for([&] { return rec->count.load() >= 32; }, 5s));
  {
    const auto n = rec->stored();
    for (std::uint32_t i = 0; i < n; i++)
      REQUIRE(rec->sizes[i].load() == 128);
  }

  // Re-pin the graph to a quantum that is not a multiple of the block size
  // while the engine runs: slices of 128 with a 64-frame tail.
  const auto before_requantum = rec->stored();
  REQUIRE(daemon.force_quantum(448));
  REQUIRE(wait_for(
      [&] {
        const auto n = rec->stored();
        for (auto i = before_requantum; i < n; i++)
          if (rec->sizes[i].load() == 64)
            return true;
        return false;
      },
      5s));
  {
    const auto n = rec->stored();
    for (std::uint32_t i = 0; i < n; i++)
    {
      const auto size = rec->sizes[i].load();
      REQUIRE(size <= 128u);
      REQUIRE(size > 0u);
    }
  }

  // Release the global force: the engine's own node.force-quantum takes
  // over again and the graph returns to the requested 128. Wait for the
  // 448-quantum tail slices (64) to stop appearing, then require a run of
  // ticks that are all exactly 128 — 16 more ticks of any size would also
  // pass with a graph stuck at 448, which is not a recovery.
  REQUIRE(daemon.force_quantum(0));
  REQUIRE(wait_for(
      [&] {
        const auto n = rec->stored();
        if (n < 32)
          return false;
        for (auto i = n - 32; i < n; i++)
          if (rec->sizes[i].load() != 128)
            return false;
        return true;
      },
      5s));

  engine->stop();
}

TEST_CASE(
    "pipewire: the engine reports the graph's rate when its own is refused",
    "[pipewire][integration]")
{
  daemon_fixture daemon;
  if (!daemon.start())
    SKIP("cannot start a private pipewire daemon");

  // Pin the graph rate globally: node.force-rate of every client is
  // ignored. Historically the engine still claimed effective_sample_rate =
  // requested rate, so the host resampled soundfiles for a rate that was
  // not being played.
  if (!daemon.force_rate(48000))
    SKIP("pw-metadata is unavailable");

  auto inst = libremidi::pipewire::shared_instance();
  if (!inst)
    SKIP("libpipewire is unavailable");
  auto ctx = libremidi::pipewire::context::make(inst);
  if (!ctx || !ctx->ok())
    SKIP("cannot connect to the private pipewire daemon");

  ossia::audio_setup setup;
  setup.name = "ossia-rate-test";
  setup.inputs = {"in_l", "in_r"};
  setup.outputs = {"out_l", "out_r"};
  setup.rate = 44100;
  setup.buffer_size = 128;

  auto engine = std::make_shared<ossia::pipewire_audio_protocol>(ctx, setup);
  REQUIRE(engine->running());

  auto rec = std::make_shared<tick_recorder>();
  engine->set_tick([rec](const ossia::audio_tick_state& st) {
    rec->push(static_cast<std::uint32_t>(st.frames));
  });
  REQUIRE(wait_for([&] { return rec->count.load() >= 8; }, 5s));

  // The truth, not the request: the host configures itself from this.
  REQUIRE(engine->effective_sample_rate == 48000);

  engine->stop();
}

TEST_CASE(
    "pipewire: the requested rate is granted when nothing overrides it",
    "[pipewire][integration]")
{
  daemon_fixture daemon;
  if (!daemon.start())
    SKIP("cannot start a private pipewire daemon");

  auto inst = libremidi::pipewire::shared_instance();
  if (!inst)
    SKIP("libpipewire is unavailable");
  auto ctx = libremidi::pipewire::context::make(inst);
  if (!ctx || !ctx->ok())
    SKIP("cannot connect to the private pipewire daemon");

  // 44100 is not in the daemon's allowed-rates (default: 48000 only):
  // node.force-rate must still win, replacing the allowed list.
  ossia::audio_setup setup;
  setup.name = "ossia-rate-test-free";
  setup.inputs = {"in_l", "in_r"};
  setup.outputs = {"out_l", "out_r"};
  setup.rate = 44100;
  setup.buffer_size = 128;

  auto engine = std::make_shared<ossia::pipewire_audio_protocol>(ctx, setup);
  REQUIRE(engine->running());

  auto rec = std::make_shared<tick_recorder>();
  engine->set_tick([rec](const ossia::audio_tick_state& st) {
    rec->push(static_cast<std::uint32_t>(st.frames));
  });
  REQUIRE(wait_for([&] { return rec->count.load() >= 8; }, 5s));

  // Cycles flowed, so the constructor observed a real graph rate; it must
  // have been the requested one.
  REQUIRE(engine->effective_sample_rate == 44100);

  engine->stop();
}

#endif
