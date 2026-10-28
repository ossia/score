// A tick split at a date, as a process does to act on a grid point inside it,
// carries the musical span of each part. A date is relative to the item
// ticked: in a box that starts late in a scenario, the musical position of
// the timeline is not proportional to it.

#include <ossia/dataflow/token_request.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Catch::Approx;

namespace
{
constexpr int64_t ms = 705600; // flicks

//! A tick of a box that starts at `box_start` ms of the timeline, over its
//! dates [prev; date] ms, at 120 bpm: the timeline's quarters.
ossia::token_request box_tick(double box_start, double prev, double date)
{
  ossia::token_request tk;
  tk.prev_date = ossia::time_value{int64_t(prev * ms)};
  tk.date = ossia::time_value{int64_t(date * ms)};
  tk.tempo = 120.;
  tk.start_sample = 0;
  tk.length_sample = 480;
  tk.musical_start_position = (box_start + prev) / 500.;
  tk.musical_end_position = (box_start + date) / 500.;
  tk.musical_start_last_bar = std::floor(tk.musical_start_position / 4.) * 4.;
  tk.musical_end_last_bar = std::floor(tk.musical_end_position / 4.) * 4.;
  return tk;
}
}

TEST_CASE("A split tick has the musical positions of its dates", "[ossia][token_request]")
{
  for(const double box_start : {0., 7000., 123456.})
  {
    INFO("box at " << box_start << " ms");
    {
      auto tk = box_tick(box_start, 1490., 1510.);
      tk.set_start_time(ossia::time_value{1500 * ms});
      CHECK(tk.musical_start_position == Approx((box_start + 1500.) / 500.));
      CHECK(tk.musical_end_position == Approx((box_start + 1510.) / 500.));
      CHECK(tk.start_sample == 240);
      CHECK(tk.length_sample == 240);
    }
    {
      auto tk = box_tick(box_start, 1490., 1510.);
      tk.set_end_time(ossia::time_value{1500 * ms});
      CHECK(tk.musical_start_position == Approx((box_start + 1490.) / 500.));
      CHECK(tk.musical_end_position == Approx((box_start + 1500.) / 500.));
      CHECK(tk.length_sample == 240);
    }
    {
      // The first tick of the box starts at its date 0.
      auto tk = box_tick(box_start, 0., 20.);
      tk.set_start_time(ossia::time_value{5 * ms});
      CHECK(tk.musical_start_position == Approx((box_start + 5.) / 500.));
    }
  }
}

TEST_CASE("A split tick played backwards has the musical positions of its dates",
          "[ossia][token_request]")
{
  auto tk = box_tick(7000., 1510., 1490.);
  tk.speed = -1.;
  auto first = tk;
  first.set_end_time(ossia::time_value{1500 * ms});
  CHECK(first.musical_start_position == Approx(8510. / 500.));
  CHECK(first.musical_end_position == Approx(8500. / 500.));
  auto second = tk;
  second.set_start_time(ossia::time_value{1500 * ms});
  CHECK(second.musical_start_position == Approx(8500. / 500.));
  CHECK(second.musical_end_position == Approx(8490. / 500.));
}
