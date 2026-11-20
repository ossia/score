// The transport's bar readout and the layout of its pulse grid.

#include <Scenario/Application/Menus/MusicalPosition.hpp>

#include <catch2/catch_test_macros.hpp>

#include <utility>
#include <vector>

using Scenario::musicalPosition;
using Scenario::pulseGridLayout;

namespace
{
using Signatures = std::vector<std::pair<TimeVal, ossia::time_signature>>;
constexpr int64_t quarter = ossia::quarter_duration<int64_t>;
constexpr int64_t whole = 4 * quarter;

std::vector<int> rowsOf(int numerator)
{
  const auto g = pulseGridLayout(numerator);
  std::vector<int> rows;
  for(int r = 0; r < g.rows; r++)
    rows.push_back(g.pulsesInRow(r));
  return rows;
}
}

TEST_CASE("pulse grid: common and odd numerators", "[transport]")
{
  using V = std::vector<int>;
  CHECK(rowsOf(1) == V{1});
  CHECK(rowsOf(2) == V{2});
  CHECK(rowsOf(3) == V{2, 1});
  CHECK(rowsOf(4) == V{2, 2});
  CHECK(rowsOf(5) == V{3, 2});
  CHECK(rowsOf(6) == V{3, 3});
  CHECK(rowsOf(7) == V{3, 3, 1});
  CHECK(rowsOf(8) == V{3, 3, 2});
  CHECK(rowsOf(9) == V{3, 3, 3});
  CHECK(rowsOf(11) == V{4, 4, 3});
  CHECK(rowsOf(12) == V{4, 4, 4});
  CHECK(rowsOf(13) == V{4, 4, 4, 1});
  CHECK(rowsOf(16) == V{4, 4, 4, 4});
  CHECK(rowsOf(19) == V{5, 5, 5, 4});
}

TEST_CASE("pulse grid: every numerator fits its smallest square", "[transport]")
{
  for(int n = -1; n <= 1024; n++)
  {
    INFO("numerator " << n);
    const auto g = pulseGridLayout(n);
    const int pulses = n < 1 ? 1 : n;
    REQUIRE(g.pulses == pulses);
    REQUIRE(g.columns >= 1);
    REQUIRE(g.rows >= 1);

    // Never taller than wide: the cells are sized from the column count.
    CHECK(g.rows <= g.columns);
    // The smallest square holding the pulses.
    CHECK(g.columns * g.columns >= pulses);
    CHECK((g.columns - 1) * (g.columns - 1) < pulses);

    // Row major from the top left, no hole before the last pulse.
    int total = 0;
    for(int r = 0; r < g.rows; r++)
    {
      const int in_row = g.pulsesInRow(r);
      CHECK(in_row >= 1);
      CHECK(in_row <= g.columns);
      if(r < g.rows - 1)
        CHECK(in_row == g.columns);
      total += in_row;
    }
    CHECK(total == pulses);

    CHECK(g.row(0) == 0);
    CHECK(g.column(0) == 0);
    CHECK(g.row(pulses - 1) == g.rows - 1);
    CHECK(g.column(pulses - 1) == g.pulsesInRow(g.rows - 1) - 1);
  }
}

TEST_CASE("musical position: 4/4 without any signature", "[transport]")
{
  const Signatures none;
  auto p = musicalPosition(none, TimeVal{0});
  CHECK(p.bar == 0);
  CHECK(p.pulse == 0);
  CHECK(p.subdivision == 0);
  CHECK(p.signature == ossia::time_signature{4, 4});

  // Bar 3, third beat, second sixteenth.
  p = musicalPosition(none, TimeVal{2 * whole + 2 * quarter + quarter / 4});
  CHECK(p.bar == 2);
  CHECK(p.pulse == 2);
  CHECK(p.subdivision == 1);

  p = musicalPosition(none, TimeVal{whole - 1});
  CHECK(p.bar == 0);
  CHECK(p.pulse == 3);
  CHECK(p.subdivision == 3);
}

TEST_CASE("musical position: pulses are in units of the denominator", "[transport]")
{
  const Signatures seven_eight{{TimeVal{0}, {7, 8}}};
  const int64_t eighth = quarter / 2;
  for(int bar = 0; bar < 3; bar++)
  {
    for(int pulse = 0; pulse < 7; pulse++)
    {
      const auto p
          = musicalPosition(seven_eight, TimeVal{(bar * 7 + pulse) * eighth + 1});
      CHECK(p.bar == bar);
      CHECK(p.pulse == pulse);
      CHECK(p.signature == ossia::time_signature{7, 8});
    }
  }

  const Signatures nineteen{{TimeVal{0}, {19, 16}}};
  const int64_t sixteenth = quarter / 4;
  for(int pulse = 0; pulse < 19; pulse++)
  {
    const auto p = musicalPosition(nineteen, TimeVal{(19 + pulse) * sixteenth});
    CHECK(p.bar == 1);
    CHECK(p.pulse == pulse);
    CHECK(p.subdivision == 0);
  }
}

TEST_CASE("musical position: the pulse stays inside the bar", "[transport]")
{
  // Denominators that do not divide a whole note in flicks evenly.
  for(uint16_t lower : {1, 2, 3, 4, 5, 7, 8, 16, 32, 64})
  {
    for(uint16_t upper : {1, 3, 4, 7, 19, 64})
    {
      const Signatures sigs{{TimeVal{0}, {upper, lower}}};
      const int64_t bar = whole * upper / lower;
      for(int64_t t : {int64_t(0), bar - 1, bar, 2 * bar - 1, 5 * bar + bar / 2})
      {
        INFO(upper << "/" << lower << " at " << t);
        const auto p = musicalPosition(sigs, TimeVal{t});
        CHECK(p.bar == t / bar);
        CHECK(p.pulse >= 0);
        CHECK(p.pulse < upper);
        CHECK(p.subdivision >= 0);
        CHECK(p.subdivision < 4);
      }
    }
  }
}

TEST_CASE("musical position: signature changes", "[transport]")
{
  // Two bars of 4/4, then 7/8 starting half-way through what would have been
  // the fourth bar of 3/4, then 19/16.
  const Signatures sigs{
      {TimeVal{0}, {4, 4}},
      {TimeVal{2 * whole}, {3, 4}},
      {TimeVal{2 * whole + 4 * quarter}, {7, 8}},
      {TimeVal{10 * whole}, {19, 16}}};

  auto p = musicalPosition(sigs, TimeVal{2 * whole - 1});
  CHECK(p.bar == 1);
  CHECK(p.signature == ossia::time_signature{4, 4});

  p = musicalPosition(sigs, TimeVal{2 * whole});
  CHECK(p.bar == 2);
  CHECK(p.pulse == 0);
  CHECK(p.signature == ossia::time_signature{3, 4});

  // The 3/4 section lasts four quarters: one bar and a cut one.
  p = musicalPosition(sigs, TimeVal{2 * whole + 3 * quarter});
  CHECK(p.bar == 3);
  CHECK(p.pulse == 0);

  p = musicalPosition(sigs, TimeVal{2 * whole + 4 * quarter});
  CHECK(p.bar == 4);
  CHECK(p.pulse == 0);
  CHECK(p.signature == ossia::time_signature{7, 8});

  p = musicalPosition(sigs, TimeVal{2 * whole + 4 * quarter + 5 * (quarter / 2)});
  CHECK(p.bar == 4);
  CHECK(p.pulse == 5);

  p = musicalPosition(sigs, TimeVal{10 * whole});
  CHECK(p.signature == ossia::time_signature{19, 16});
  CHECK(p.pulse == 0);
  const auto first_19 = p.bar;
  p = musicalPosition(sigs, TimeVal{10 * whole + 19 * (quarter / 4) + 18 * (quarter / 4)});
  CHECK(p.bar == first_19 + 1);
  CHECK(p.pulse == 18);
}

TEST_CASE("musical position: before the first signature", "[transport]")
{
  const Signatures late{{TimeVal{whole}, {7, 8}}};
  auto p = musicalPosition(late, TimeVal{quarter});
  CHECK(p.bar == 0);
  CHECK(p.pulse == 1);
  CHECK(p.signature == ossia::time_signature{4, 4});

  p = musicalPosition(late, TimeVal{whole});
  CHECK(p.bar == 1);
  CHECK(p.signature == ossia::time_signature{7, 8});

  p = musicalPosition(late, TimeVal{-quarter});
  CHECK(p.bar == 0);
  CHECK(p.pulse == 0);
}
