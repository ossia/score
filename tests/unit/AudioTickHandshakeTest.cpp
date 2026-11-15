// audio_engine::set_tick() hands a tick to the audio thread and returns once
// that thread runs it: callers then free what the previous tick used (the
// execution's graph, a document). A previous tick that still runs after
// set_tick() returned is a use-after-free in the making.

#include <ossia/audio/audio_engine.hpp>
#include <ossia/detail/logger.hpp>

#include <catch2/catch_all.hpp>

#include <atomic>
#include <chrono>
#include <thread>

namespace
{
//! An engine whose "audio thread" calls back as fast as it can, so that the
//! handshake is exercised millions of times.
struct spinning_engine final : ossia::audio_engine
{
  std::atomic_bool quit{};
  std::atomic_bool patient{};
  std::thread thread;

  spinning_engine()
  {
    effective_sample_rate = 48000;
    effective_buffer_size = 16;
    thread = std::thread{[this] {
      float* none[1]{};
      while(!quit)
      {
        tick_start();
        if(stop_processing)
        {
          tick_clear();
          continue;
        }
        ossia::audio_tick_state st{none, none, 0, 0, 16, 0.};
        if(audio_tick)
          audio_tick(st);
        tick_end();
      }
    }};
  }
  ~spinning_engine()
  {
    quit = true;
    thread.join();
  }
  bool running() const override { return !quit; }
  // The real engines sleep for a buffer. Yielding instead makes the main
  // thread check the reply right after the audio thread wrote it, which is
  // where a premature reply shows; it also makes sync() give up sometimes,
  // which set_tick() then reports. A patient engine sleeps, as they do.
  void wait(int ms) override
  {
    if(patient)
      std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    else
      std::this_thread::yield();
  }
};

std::atomic_int g_oldest_allowed{0};
std::atomic_int g_stale_runs{0};

struct NumberedTick
{
  int id{};
  void operator()(const ossia::audio_tick_state&) const
  {
    if(id < g_oldest_allowed.load(std::memory_order_acquire))
      g_stale_runs.fetch_add(1, std::memory_order_relaxed);
  }
};
}

TEST_CASE("Once set_tick() returned, the previous tick never runs again", "[audio][tick]")
{
  // sync() giving up is expected here, and logged every time
  ossia::logger().set_level(spdlog::level::off);
  spinning_engine engine;
  g_stale_runs = 0;
  g_oldest_allowed = 0;

  int unacknowledged = 0;
  for(int i = 1; i <= 2000000; i++)
  {
    // Only an acknowledged swap promises anything about the previous tick
    if(engine.set_tick(NumberedTick{i}))
      g_oldest_allowed.store(i, std::memory_order_release);
    else
      unacknowledged++;
  }
  // A yielding sync() gives up within microseconds, as soon as the audio
  // thread is descheduled: the last swap waits as the real engines do, for
  // a few seconds in all on a loaded machine
  engine.patient = true;
  bool parked = false;
  for(int i = 0; i < 100 && !parked; i++)
    parked = engine.set_tick({});
  CHECK(parked);

  CHECK(g_stale_runs.load() == 0);
  // Most swaps are acknowledged: the check above is not vacuous
  CHECK(unacknowledged < 200000);
}
