// The count-in of a recording (Recording::count_in_node): the metronome clicks
// the beats of the box, on the box's clock, while the takes wait, and is
// silent once they record.

#include <Recording/Engine/CountIn.hpp>

#include <ossia/dataflow/exec_state_facade.hpp>
#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/nodes/forward_node.hpp>
#include <ossia/dataflow/audio_port.hpp>

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <cmath>

using namespace Recording;

namespace
{
constexpr int rate = 48000;
constexpr int buffer = 480;
constexpr int64_t flicks_per_second = 705600000;
constexpr int64_t tick_flicks = flicks_per_second / 100;
constexpr double Q = 352800000.0;
}

TEST_CASE("The count-in clicks the beats while the takes wait, then stops", "[recording][countin]")
{
  ossia::execution_state st;
  st.sampleRate = rate;
  st.bufferSize = buffer;
  st.modelToSamplesRatio = double(rate) / flicks_per_second;
  st.samplesToModelRatio = flicks_per_second / double(rate);

  auto clock = std::make_shared<ossia::nodes::forward_node>();
  std::atomic_bool waiting{true};
  count_in_node node{rate, clock, [&] { return waiting.load(); }};

  int64_t date = 0;
  auto tick = [&] {
    clock->requested_tokens.clear();
    ossia::token_request tk;
    tk.prev_date = ossia::time_value{date};
    tk.date = ossia::time_value{date + tick_flicks};
    tk.speed = 1.;
    tk.tempo = 120.;
    tk.signature = ossia::time_signature{4, 4};
    tk.start_sample = 0;
    tk.length_sample = buffer;
    tk.musical_start_position = date / Q;
    tk.musical_end_position = (date + tick_flicks) / Q;
    tk.musical_start_last_bar = std::floor(tk.musical_start_position / 4.) * 4.;
    tk.musical_end_last_bar = std::floor(tk.musical_end_position / 4.) * 4.;
    clock->requested_tokens.push_back(tk);
    auto& out = *node.metro.audio_out.target<ossia::audio_port>();
    for(auto& ch : out.get())
      std::fill(ch.begin(), ch.end(), 0.);
    ossia::graph_node& n = node;
    n.run(ossia::token_request{}, ossia::exec_state_facade{&st});
    date += tick_flicks;
    double energy = 0.;
    for(auto& ch : out.get())
      for(double v : ch)
        energy += v * v;
    return energy;
  };

  // The first beat at 0, the second at half a second (tick 50)
  CHECK(tick() > 0.);
  for(int i = 1; i < 50; i++)
    tick();
  CHECK(tick() > 0.);

  // Recording: silent, beats or not
  waiting = false;
  for(int i = 51; i < 100; i++)
    tick();
  CHECK(tick() == 0.);
}
