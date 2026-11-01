// The LED View's decoding: component orders, value ranges, gamma, how
// pixels are wired across a grid, and where DMX universes start.

#include <UI/LEDView.hpp>

#include <catch2/catch_all.hpp>

using namespace uo;

TEST_CASE("LED View: components are read in the given order", "[led_view]")
{
  const std::vector<float> grb{255, 0, 0}; // full green on a GRB strip
  const auto c = ledColors(grb, RGB, LEDOrder::GRB, 1.f);
  REQUIRE(c.size() == 1);
  CHECK(c[0] == QColor(0, 255, 0));
  CHECK(ledColors(grb, RGB, LEDOrder::RGB, 1.f)[0] == QColor(255, 0, 0));
  CHECK(ledColors({0, 0, 255}, RGB, LEDOrder::BGR, 1.f)[0] == QColor(255, 0, 0));
}

TEST_CASE("LED View: value ranges and gamma", "[led_view]")
{
  CHECK(ledColors({1.f, 0.f, 0.f}, RGB01, LEDOrder::RGB, 1.f)[0] == QColor(255, 0, 0));
  CHECK(ledColors({65535.f, 0.f, 0.f}, RGB16, LEDOrder::RGB, 1.f)[0] == QColor(255, 0, 0));
  CHECK(ledColors({0.5f}, Lightness01, LEDOrder::RGB, 1.f).size() == 1);
  // A value meant for an LED with gamma 2.2 shows brighter on screen
  const auto lin = ledColors({0.25f}, Lightness01, LEDOrder::RGB, 1.f)[0];
  const auto corrected = ledColors({0.25f}, Lightness01, LEDOrder::RGB, 2.2f)[0];
  CHECK(corrected.red() > lin.red());
  // A partial pixel at the end is left out
  CHECK(ledColors({1, 2, 3, 4}, RGB, LEDOrder::RGB, 1.f).size() == 1);
  CHECK(ledColors({0, 0, 0, 255}, RGBW, LEDOrder::RGB, 1.f)[0].red() == 255);
}

TEST_CASE("LED View: serpentine wiring turns back on every other row", "[led_view]")
{
  using P = std::pair<int, int>;
  CHECK(ledCell(0, 4, 3, LEDLayout::Rows) == P{0, 0});
  CHECK(ledCell(5, 4, 3, LEDLayout::Rows) == P{1, 1});
  CHECK(ledCell(4, 4, 3, LEDLayout::SerpentineRows) == P{3, 1});
  CHECK(ledCell(7, 4, 3, LEDLayout::SerpentineRows) == P{0, 1});
  CHECK(ledCell(8, 4, 3, LEDLayout::SerpentineRows) == P{0, 2});
  CHECK(ledCell(3, 4, 3, LEDLayout::Columns) == P{1, 0});
  CHECK(ledCell(3, 4, 3, LEDLayout::SerpentineColumns) == P{1, 2});
}

TEST_CASE("LED View: universes hold whole pixels", "[led_view]")
{
  using P = std::pair<int, int>;
  CHECK(ledSlot(0, 3) == P{1, 1});
  CHECK(ledSlot(169, 3) == P{1, 508});
  CHECK(ledSlot(170, 3) == P{2, 1});
  CHECK(ledSlot(128, 4) == P{2, 1});
  CHECK(ledSlot(511, 1) == P{1, 512});
}
