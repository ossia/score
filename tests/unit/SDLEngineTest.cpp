// The SDL audio engine across restarts: score stops the running engine,
// starts the new one, and only later destroys the stopped one (it must have
// released the device before the new one opens it, and its last callback must
// be over before it is freed).
//
// SDL's own dummy / disk drivers, so this runs headless everywhere.

#include <ossia/audio/sdl_protocol.hpp>

#include <catch2/catch_all.hpp>

#if defined(OSSIA_AUDIO_SDL)
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <thread>

namespace
{
using namespace std::chrono_literals;

struct Counter
{
  std::atomic_int* n;
  void operator()(const ossia::audio_tick_state&) const { (*n)++; }
};

void use_driver(const char* driver)
{
  // Only taken into account when SDL's audio subsystem is (re)initialized
  REQUIRE(SDL_WasInit(SDL_INIT_AUDIO) == 0);
  setenv("SDL_AUDIODRIVER", driver, 1);
}
}

TEST_CASE("Destroying the previous SDL engine leaves the new one running", "[audio][sdl]")
{
  use_driver("dummy");
  {
    // What restart_engine() does: stop, start the new one, reap the old one
    auto previous = std::make_unique<ossia::sdl_protocol>(48000, 256);
    REQUIRE(previous->running());
    previous->stop();

    auto current = std::make_unique<ossia::sdl_protocol>(48000, 256);
    REQUIRE(current->running());
    std::atomic_int callbacks{0};
    current->set_tick(Counter{&callbacks});

    previous.reset();

    CHECK(current->running());
    CHECK(SDL_WasInit(SDL_INIT_AUDIO) != 0);
    const int before = callbacks.load();
    std::this_thread::sleep_for(200ms);
    CHECK(callbacks.load() > before);

    current->set_tick({});
  }
  // Every engine gone: none of them keeps the audio subsystem alive
  CHECK(SDL_WasInit(SDL_INIT_AUDIO) == 0);
}

TEST_CASE("An SDL engine that cannot open its device does not leak SDL", "[audio][sdl]")
{
  // The disk driver initializes, then cannot open a file it cannot create
  use_driver("disk");
  setenv("SDL_DISKAUDIOFILE", "/nonexistent-directory/ossia-test.raw", 1);
  CHECK_THROWS_AS(ossia::sdl_protocol(48000, 256), std::runtime_error);
  CHECK(SDL_WasInit(SDL_INIT_AUDIO) == 0);
  unsetenv("SDL_DISKAUDIOFILE");
}
#endif
