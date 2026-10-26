// halp::next_grid_point: where the next point of a grid chooser's grid falls
// in a buffer, for every unit. Musical grids are checked against the
// positions a halp::tick_musical describes, the others against frame counts.

#include <catch2/catch_test_macros.hpp>
#include <halp/grid.hpp>

#include <cmath>

namespace
{
//! A tick from quarter `start` to `end` over `frames` frames, in num/denom
//! since the signature change at `sig_change`.
halp::tick_musical tick(
    double start, double end, int frames, int num = 4, int denom = 4,
    double sig_change = 0., int64_t position_in_frames = 0)
{
  halp::tick_musical tk;
  tk.frames = frames;
  tk.signature = {num, denom};
  tk.start_position_in_quarters = start;
  tk.end_position_in_quarters = end;
  tk.last_signature_change = sig_change;
  tk.position_in_frames = position_in_frames;
  const double bar = 4. * num / denom;
  tk.bar_at_start = sig_change + std::floor((start - sig_change) / bar) * bar;
  tk.bar_at_end = sig_change + std::floor((end - sig_change) / bar) * bar;
  return tk;
}

using halp::grid_value;
using halp::next_grid_point;
constexpr double sr = 48000.;
}

TEST_CASE("Grid: one bar in 4/4 and 3/4", "[halp][grid]")
{
  // The bar line at quarter 4, half-way through
  CHECK(next_grid_point(grid_value::bars(1), tick(3.5, 4.5, 100), sr) == 50);
  // In 3/4 the bar line is at 3
  CHECK(next_grid_point(grid_value::bars(1), tick(2.5, 3.5, 100, 3, 4), sr) == 50);
  CHECK(
      next_grid_point(grid_value::bars(1), tick(3.5, 4.5, 100, 3, 4), sr)
      == std::nullopt);
  // A point on the first frame belongs to this buffer, one on the end to the next
  CHECK(next_grid_point(grid_value::bars(1), tick(4., 5., 100), sr) == 0);
  CHECK(next_grid_point(grid_value::bars(1), tick(3., 4., 100), sr) == std::nullopt);
}

TEST_CASE("Grid: several bars count from the signature change", "[halp][grid]")
{
  // 3 bars of 4/4: 0, 12, 24
  CHECK(next_grid_point(grid_value::bars(3), tick(11.5, 12.5, 100), sr) == 50);
  CHECK(next_grid_point(grid_value::bars(3), tick(7.5, 8.5, 100), sr) == std::nullopt);
  // 3 bars of 3/4 from a change at quarter 2: 2, 11, 20
  CHECK(next_grid_point(grid_value::bars(3), tick(10.5, 11.5, 100, 3, 4, 2.), sr) == 50);
  CHECK(
      next_grid_point(grid_value::bars(3), tick(7.5, 8.5, 100, 3, 4, 2.), sr)
      == std::nullopt);
}

TEST_CASE("Grid: note values", "[halp][grid]")
{
  // A quarter: the next one at 1
  CHECK(next_grid_point(grid_value::note(0.25), tick(0.25, 1.25, 100), sr) == 75);
  CHECK(
      next_grid_point(grid_value::note(0.25), tick(0.1, 0.9, 100), sr) == std::nullopt);
  // An eighth triplet (1/12 of a whole note, a third of a quarter): 0, 1/3
  CHECK(next_grid_point(grid_value::note(1. / 12.), tick(0., 0.5, 600), sr) == 0);
  CHECK(next_grid_point(grid_value::note(1. / 12.), tick(0.1, 0.5, 400), sr) == 233);
  // A dotted quarter in 4/4: 0, 1.5, 3, and the bar line restarts it at 4,
  // not 4.5
  CHECK(next_grid_point(grid_value::note(3. / 8.), tick(3.2, 4.2, 100), sr) == 80);
  CHECK(next_grid_point(grid_value::note(3. / 8.), tick(4.2, 5.7, 150), sr) == 130);
}

TEST_CASE("Grid: a whole note is four quarters, not a bar", "[halp][grid]")
{
  // In 3/4 a whole note is longer than a bar: it counts from the signature
  // change, 0, 4, 8, across the bar lines at 3 and 6.
  CHECK(next_grid_point(grid_value::note(1.), tick(3.5, 4.5, 100, 3, 4), sr) == 50);
  CHECK(
      next_grid_point(grid_value::note(1.), tick(2.5, 3.5, 100, 3, 4), sr)
      == std::nullopt);
  // Two whole notes in 4/4: every other bar
  CHECK(next_grid_point(grid_value::note(2.), tick(7.5, 8.5, 100), sr) == 50);
  CHECK(next_grid_point(grid_value::note(2.), tick(3.5, 4.5, 100), sr) == std::nullopt);
}

TEST_CASE("Grid: musical grids wait while the transport is stopped", "[halp][grid]")
{
  CHECK(next_grid_point(grid_value::bars(1), tick(4., 4., 100), sr) == std::nullopt);
  CHECK(next_grid_point(grid_value::note(0.25), tick(5., 4., 100), sr) == std::nullopt);
}

TEST_CASE("Grid: periods", "[halp][grid]")
{
  const auto loop = [](int64_t position) {
    return halp::grid_period{.length = 1000, .position = position};
  };
  // Each restart
  CHECK(next_grid_point(grid_value::period(1), tick(0, 1, 100), sr, loop(950)) == 50);
  CHECK(next_grid_point(grid_value::period(1), tick(0, 1, 100), sr, loop(1000)) == 0);
  CHECK(
      next_grid_point(grid_value::period(1), tick(0, 1, 100), sr, loop(400))
      == std::nullopt);
  // Twice per loop: 0, 500, 1000
  CHECK(next_grid_point(grid_value::period(0.5), tick(0, 1, 100), sr, loop(450)) == 50);
  CHECK(
      next_grid_point(grid_value::period(0.5), tick(0, 1, 100), sr, loop(400))
      == std::nullopt);
  // Every other restart: 0, 2000, 4000
  CHECK(
      next_grid_point(grid_value::period(2), tick(0, 1, 100), sr, loop(950))
      == std::nullopt);
  CHECK(next_grid_point(grid_value::period(2), tick(0, 1, 100), sr, loop(1950)) == 50);
  // A quarter of a 1000-frame loop is 250 frames: 750 then 1000
  CHECK(next_grid_point(grid_value::period(0.25), tick(0, 1, 100), sr, loop(700)) == 50);
  // No period, no grid
  CHECK(next_grid_point(grid_value::period(1), tick(0, 1, 100), sr) == std::nullopt);
}

TEST_CASE("Grid: free times and none", "[halp][grid]")
{
  // None: no quantization, now
  CHECK(next_grid_point(grid_value::seconds(0), tick(0.3, 0.4, 100), sr) == 0);
  CHECK(grid_value{}.none());
  CHECK(!grid_value::note(0.25).none());

  // Half a second from the start of playback: 24000 frames
  CHECK(
      next_grid_point(grid_value::seconds(0.5), tick(0, 1, 100, 4, 4, 0., 23950), sr)
      == 50);
  CHECK(
      next_grid_point(grid_value::seconds(0.5), tick(0, 1, 100, 4, 4, 0., 24100), sr)
      == std::nullopt);
  // From the origin of the period when there is one
  CHECK(
      next_grid_point(
          grid_value::seconds(0.5), tick(0, 1, 100, 4, 4, 0., 5), sr,
          {.length = 96000, .position = 23990})
      == 10);
  // The parent's grid is not resolved by the object
  CHECK(
      next_grid_point(grid_value{halp::grid_unit::parent, 1.}, tick(3.5, 4.5, 100), sr)
      == std::nullopt);
}

TEST_CASE("Grid: what a time chooser held", "[halp][grid]")
{
  CHECK(grid_value::from_time_chooser(0.125, true) == grid_value::note(0.125));
  CHECK(grid_value::from_time_chooser(0.3, false) == grid_value::seconds(0.3));
  CHECK(halp::grid_unit_flag(halp::grid_unit::bars) == halp::grid_units::bars);
}

namespace
{
struct GridSpec
{
  halp::grid_chooser<
      "Quantize",
      halp::grid_setup{
          .units = halp::grid_units::musical | halp::grid_units::period,
          .init = halp::grid_value::bars(2),
          .max_bars = 8}>
      quantize;
  halp::grid_chooser<"Default"> plain;
};
}

TEST_CASE("Grid chooser declaration", "[halp][grid]")
{
  GridSpec s;
  CHECK(s.quantize.value == grid_value::bars(2));
  STATIC_REQUIRE(decltype(s.quantize)::grid().max_bars == 8);
  STATIC_REQUIRE(decltype(s.quantize)::period_name() == "loop");
  STATIC_REQUIRE(decltype(s.quantize)::name() == "Quantize");
  STATIC_REQUIRE((decltype(s.plain)::grid().units & halp::grid_units::period) == 0);
  CHECK(s.plain.value == grid_value::note(0.25));
}
