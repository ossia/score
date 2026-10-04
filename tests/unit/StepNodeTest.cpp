// The step sequencer's engine, driven directly: steps in seconds and synced to
// the tempo, several sequences and the quantized switch between them,
// rewinding, and an empty sequence.

#include <Media/Step/StepNode.hpp>

#include <ossia/dataflow/exec_state_facade.hpp>
#include <ossia/dataflow/execution_state.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

namespace
{
using Media::Step::step_node;

struct fixture
{
  step_node node;
  ossia::execution_state st; // modelToSamplesRatio == 1

  fixture()
  {
    node.sequences = {{1.f, 2.f, 3.f, 4.f}};
    node.step_duration = 1.; // seconds
    node.synced = false;
  }

  //! Runs [prev; date] (in model units: flicks, the ratio is 1) with the
  //! musical positions of 4/4 at 1000 units per quarter, and returns what the
  //! outlet sent.
  std::vector<std::pair<int64_t, float>> tick(int64_t prev, int64_t date)
  {
    auto& port = *node.out.target<ossia::value_port>();
    port.get_data().clear();

    ossia::exec_state_facade fac{&st};
    ossia::token_request tk{
        ossia::time_value{prev},
        ossia::time_value{date},
        ossia::time_value{1'000'000'000'000},
        ossia::time_value{0},
        date >= prev ? 1. : -1.,
        ossia::time_signature{4, 4},
        120.};
    tk.musical_start_position = prev / 1000.;
    tk.musical_end_position = date / 1000.;
    tk.musical_start_last_bar = std::floor(tk.musical_start_position / 4.) * 4.;
    tk.musical_end_last_bar = std::floor(tk.musical_end_position / 4.) * 4.;
    static_cast<ossia::graph_node&>(node).run(tk, fac);

    std::vector<std::pair<int64_t, float>> res;
    for(auto& v : port.get_data())
      res.emplace_back(v.timestamp, ossia::convert<float>(v.value));
    return res;
  }

  std::vector<float> values(int64_t prev, int64_t date)
  {
    std::vector<float> res;
    for(auto& [t, v] : tick(prev, date))
      res.push_back(v);
    return res;
  }
};

constexpr int64_t second = ossia::flicks_per_second<int64_t>;
}

TEST_CASE("step sequencer: a step every duration, in seconds", "[step]")
{
  fixture f;
  // 1 s steps: at 0, 1, 2, ... s from the start of the interval.
  CHECK(f.values(0, second / 2) == std::vector<float>{1.f});
  CHECK(f.values(second / 2, second).empty());
  // A tick spanning several steps plays them all, in order, and wraps.
  CHECK(f.values(second, 5 * second) == std::vector<float>{2.f, 3.f, 4.f, 1.f});
  auto t = f.tick(5 * second, 6 * second);
  REQUIRE(t.size() == 1);
  CHECK(t[0].second == 2.f);
  CHECK(t[0].first == 0); // at the start of the tick
}

TEST_CASE("step sequencer: synced steps follow the musical grid", "[step]")
{
  fixture f;
  // A 1/4 note: 1000 units here.
  f.node.synced = true;
  f.node.step_duration = 0.25;
  CHECK(f.values(0, 2500) == std::vector<float>{1.f, 2.f, 3.f});
  CHECK(f.values(2500, 4000) == std::vector<float>{4.f});
  // A 1/16: four per quarter.
  f.node.step_duration = 1. / 16.;
  CHECK(f.values(4000, 5000).size() == 4);
}

TEST_CASE("step sequencer: the Duration inlet takes seconds or {value, mode}", "[step]")
{
  fixture f;
  f.node.set_duration(ossia::value{0.5f});
  CHECK(!f.node.synced);
  CHECK(f.node.step_duration == 0.5);
  f.node.set_duration(ossia::value{ossia::vec2f{0.125f, 1.f}});
  CHECK(f.node.synced);
  CHECK(f.node.step_duration == 0.125);
  f.node.set_duration(ossia::value{ossia::vec2f{2.f, 0.f}});
  CHECK(!f.node.synced);
  CHECK(f.node.step_duration == 2.);
}

TEST_CASE("step sequencer: nothing to play is no division by zero", "[step]")
{
  fixture f;
  f.node.step_duration = 0.;
  CHECK(f.values(0, second).empty());
  f.node.step_duration = 1.;
  f.node.sequences = {{}};
  CHECK(f.values(second, 3 * second).empty());
  f.node.sequences.clear();
  CHECK(f.values(3 * second, 5 * second).empty());
}

TEST_CASE("step sequencer: each sequence wraps on its own length", "[step]")
{
  fixture f;
  f.node.sequences = {{1.f, 2.f, 3.f}, {10.f, 20.f}};
  CHECK(f.values(0, 4 * second) == std::vector<float>{1.f, 2.f, 3.f, 1.f});
}

TEST_CASE("step sequencer: a switch waits for the quantization grid", "[step]")
{
  fixture f;
  f.node.sequences = {{1.f, 2.f, 3.f, 4.f}, {10.f, 20.f, 30.f, 40.f}};
  f.node.synced = true;
  f.node.step_duration = 0.25; // quarters
  f.node.switch_rate = 1.;     // at the bar

  CHECK(f.values(0, 2000) == std::vector<float>{1.f, 2.f});
  f.node.request_sequence(1);
  // The bar line at 4000: the steps before it from sequence 0, from it
  // sequence 1, starting at its first step.
  CHECK(f.values(2000, 6000) == std::vector<float>{3.f, 4.f, 10.f, 20.f});
  CHECK(f.node.current_sequence == 1);
}

TEST_CASE("step sequencer: a free switch happens at once, at step 0", "[step]")
{
  fixture f;
  f.node.sequences = {{1.f, 2.f, 3.f, 4.f}, {10.f, 20.f, 30.f, 40.f}};
  f.node.switch_rate = 0.;
  CHECK(f.values(0, 2 * second) == std::vector<float>{1.f, 2.f});
  f.node.request_sequence(1);
  CHECK(f.values(2 * second, 4 * second) == std::vector<float>{10.f, 20.f});
}

TEST_CASE("step sequencer: requests are clamped, the last one wins", "[step]")
{
  fixture f;
  f.node.sequences = {{1.f}, {2.f}, {3.f}};
  f.node.request_sequence(1);
  f.node.request_sequence(99);
  CHECK(f.node.pending_sequence == 2);
  // Asking for the one playing cancels the switch
  f.node.request_sequence(0);
  CHECK(f.node.pending_sequence == -1);
}

TEST_CASE("step sequencer: rewinding walks the steps backwards", "[step]")
{
  fixture f;
  CHECK(f.values(0, 3 * second) == std::vector<float>{1.f, 2.f, 3.f});
  // Back over the same ground: the same steps, the other way.
  CHECK(f.values(3 * second, 0) == std::vector<float>{3.f, 2.f, 1.f});
  CHECK(f.values(0, 2 * second) == std::vector<float>{1.f, 2.f});
}

namespace
{
//! A tick of a process that starts `m0` quarter notes into the score, at 120
//! bpm in 4/4: a quarter is half a second.
std::vector<float> realtime_values(fixture& f, int64_t prev, int64_t date, double m0)
{
  auto& port = *f.node.out.target<ossia::value_port>();
  port.get_data().clear();

  ossia::exec_state_facade fac{&f.st};
  ossia::token_request tk{
      ossia::time_value{prev},
      ossia::time_value{date},
      ossia::time_value{1'000'000'000'000},
      ossia::time_value{0},
      1.,
      ossia::time_signature{4, 4},
      120.};
  const auto quarters = [](int64_t t) { return 2. * t / double(second); };
  tk.musical_start_position = m0 + quarters(prev);
  tk.musical_end_position = m0 + quarters(date);
  tk.musical_start_last_bar = std::floor(tk.musical_start_position / 4.) * 4.;
  tk.musical_end_last_bar = std::floor(tk.musical_end_position / 4.) * 4.;
  static_cast<ossia::graph_node&>(f.node).run(tk, fac);

  std::vector<float> res;
  for(auto& v : port.get_data())
    res.push_back(ossia::convert<float>(v.value));
  return res;
}
}

TEST_CASE("step sequencer: synced steps count from the start of the process", "[step]")
{
  fixture f;
  f.node.synced = true;
  f.node.step_duration = 0.5; // a half note: a second at 120 bpm

  // The process starts on the second beat of the second bar, off the
  // half-note grid of the bars: its first step still plays as it starts.
  const double m0 = 5.;
  CHECK(realtime_values(f, 0, second / 4, m0) == std::vector<float>{1.f});
  CHECK(realtime_values(f, second / 4, second, m0).empty());
  CHECK(realtime_values(f, second, second + second / 4, m0) == std::vector<float>{2.f});

  // The transport moves it to 2.5 s: the steps of 0, 1 and 2 s are past, the
  // fourth plays at 3 s.
  CHECK(realtime_values(f, 5 * second / 2, 11 * second / 4, m0).empty());
  CHECK(realtime_values(f, 11 * second / 4, 13 * second / 4, m0) == std::vector<float>{4.f});
}
