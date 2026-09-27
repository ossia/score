// Free metronome v2 and Rate Limiter v2: their period / interval is a time
// chooser, in seconds or synced to a note value.

#include <Fx/Metro_v2.hpp>
#include <Fx/RateLimiter_v2.hpp>

#include <ossia/dataflow/execution_state.hpp>

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <vector>

namespace
{
template <typename Port>
std::unique_ptr<std::vector<int64_t>> capture(Port& port)
{
  auto out = std::make_unique<std::vector<int64_t>>();
  port.call.context = out.get();
  port.call.function = +[](void* ctx, int64_t t) {
    static_cast<std::vector<int64_t>*>(ctx)->push_back(t);
  };
  return out;
}

halp::tick_musical musical(int64_t pos, int frames, double q0, double q1, double tempo)
{
  halp::tick_musical tk{};
  tk.frames = frames;
  tk.position_in_frames = pos;
  tk.start_position_in_quarters = q0;
  tk.end_position_in_quarters = q1;
  tk.tempo = tempo;
  tk.signature = {4, 4};
  return tk;
}
}

TEST_CASE("Free metronome v2: a tick every period of seconds", "[fx][metro]")
{
  Nodes::Metro::v2::Node m;
  m.prepare({.input_channels = 0, .output_channels = 0, .frames = 512, .rate = 48000.});
  auto ticks = capture(m.outputs.out);
  m.inputs.period.value = 0.25f;
  m.inputs.period.sync = false;

  // One second in buffers of 512: ticks fall inside buffers, not only where a
  // buffer starts exactly on a multiple of the period.
  std::vector<int64_t> abs;
  for(int64_t pos = 0; pos < 48000; pos += 512)
  {
    ticks->clear();
    m(musical(pos, 512, pos / 24000., (pos + 512) / 24000., 120.));
    for(auto t : *ticks)
      abs.push_back(pos + t);
  }
  // (the last buffer ends past 48000, the fifth tick)
  CHECK(abs == std::vector<int64_t>{0, 12000, 24000, 36000, 48000});
}

TEST_CASE("Free metronome v2: synced, on the quarter notes", "[fx][metro]")
{
  Nodes::Metro::v2::Node m;
  m.prepare({.input_channels = 0, .output_channels = 0, .frames = 512, .rate = 48000.});
  auto ticks = capture(m.outputs.out);
  // A quarter at 90 BPM, as the binding hands it over: seconds, synced.
  m.inputs.period.value = 60.f / 90.f;
  m.inputs.period.sync = true;

  // From quarter 0.5 to quarter 2.5 in one tick of 1000 frames: quarters 1 and 2.
  m(musical(0, 1000, 0.5, 2.5, 90.));
  CHECK(*ticks == std::vector<int64_t>{250, 750});
}

TEST_CASE("Rate Limiter v2: at most one value per interval", "[fx][ratelimiter]")
{
  Nodes::RateLimiter::v2::Node rl;
  ossia::execution_state state;
  state.bufferSize = 1000;
  state.modelToSamplesRatio = 1.;
  state.samplesToModelRatio = 1.;
  rl.ossia_state = {&state};
  ossia::value_port in;
  rl.inputs.port.value = &in;
  std::vector<std::pair<int64_t, ossia::value>> out;
  rl.outputs.out.call.context = &out;
  rl.outputs.out.call.function = +[](void* ctx, int64_t t, ossia::value v) {
    static_cast<decltype(out)*>(ctx)->emplace_back(t, std::move(v));
  };
  rl.inputs.interval.value = 0.5f; // seconds
  rl.inputs.interval.sync = false;

  constexpr int64_t second = 705'600'000;
  auto tick = [&](int64_t from, int64_t to) {
    ossia::token_request t{};
    t.prev_date = ossia::time_value{from};
    t.date = ossia::time_value{to};
    t.speed = 1.;
    in.write_value(1.f, 0);
    rl(t);
    in.get_data().clear();
  };
  // A value every 0.1 s during one second: one goes out every 0.5 s.
  for(int i = 0; i < 10; i++)
    tick(i * second / 10, (i + 1) * second / 10);
  CHECK(out.size() == 2);
}
