// The bar lines and their sub-divisions the full view draws for the part of
// the timeline it shows, zoomed in or out.

#include <Scenario/Document/Interval/FullView/TimeSignatureItem.hpp>
#include <Scenario/Document/Interval/FullView/Timebar.hpp>
#include <Scenario/Document/Interval/TimeSignatureMap.hpp>
#include <Scenario/Document/ScenarioDocument/MusicalGrid.hpp>

#include <ossia/editor/scenario/time_signature.hpp>

#include <QApplication>

#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

namespace
{
struct grid_lines
{
  std::vector<double> bars;
  std::vector<double> subs;

  //! The lines strictly between a and b.
  static int between(const std::vector<double>& v, double a, double b)
  {
    return int(std::count_if(v.begin(), v.end(), [=](double x) { return x > a && x < b; }));
  }
  bool has_bar_at(double x) const
  {
    return std::any_of(
        bars.begin(), bars.end(), [=](double b) { return std::abs(b - x) < 0.5; });
  }
};

//! The lines of a view `width` pixels wide whose left edge is at `left`, in the
//! pixels of the interval shown, at `zoom` flicks per pixel.
grid_lines draw(
    const Scenario::TimeSignatureMap& measures, double zoom, double left, double width,
    TimeVal time_delta = TimeVal::zero())
{
  Scenario::Timebars bars;
  std::unique_ptr<Scenario::LightBars> light{new Scenario::LightBars};
  std::unique_ptr<Scenario::LighterBars> lighter{new Scenario::LighterBars};
  bars.lightBars = light.get();
  bars.lighterBars = lighter.get();

  Scenario::MusicalGrid grid{bars};
  grid.setMeasures(measures);
  const TimeVal x0 = TimeVal::fromPixels(left, zoom) + time_delta;
  grid.compute(time_delta, zoom, QRectF{left, 0., width, 100.}, x0);

  grid_lines res;
  for(auto& l : light->positions)
    res.bars.push_back(l.x1());
  for(auto& l : lighter->positions)
    res.subs.push_back(l.x1());
  return res;
}

//! The flicks of a bar of `num`/`den` at the root tempo.
double bar_flicks(int num, int den)
{
  return ossia::quarter_duration<double> * 4. * double(num) / double(den);
}
}

TEST_CASE(
    "Zoomed in on a bar line, the sub-divisions are drawn on both sides of it",
    "[scenario][musical-grid]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    for(const auto [num, den] : {std::pair{4, 4}, std::pair{3, 4}, std::pair{7, 8}})
    {
      // A bar many times wider than the view, the view centred on bar 3's line.
      for(const double bar_px : {1200., 4000., 50000.})
      {
        DYNAMIC_SECTION(num << "/" << den << ", a bar " << bar_px << " pixels wide")
        {
          Scenario::TimeSignatureMap measures;
          measures[TimeVal::zero()] = ossia::time_signature{uint16_t(num), uint16_t(den)};
          const double zoom = bar_flicks(num, den) / bar_px;
          const double width = 800.;
          const double bar3 = 2. * bar_px;
          const double left = bar3 - width / 2.;

          const auto lines = draw(measures, zoom, left, width);
          INFO(lines.bars.size() << " bar lines, " << lines.subs.size() << " sub-divisions");
          CHECK(lines.has_bar_at(bar3));
          CHECK(grid_lines::between(lines.subs, left, bar3) > 0);
          CHECK(grid_lines::between(lines.subs, bar3, left + width) > 0);
          // As many on each side: they are the same steps.
          CHECK(
              std::abs(
                  grid_lines::between(lines.subs, left, bar3)
                  - grid_lines::between(lines.subs, bar3, left + width))
              <= 1);
        }
      }
    }
  });
}

TEST_CASE(
    "In a child interval, the lines after a bar line of the score are drawn too",
    "[scenario][musical-grid]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Scenario::TimeSignatureMap measures;
    measures[TimeVal::zero()] = ossia::time_signature{4, 4};
    const double bar_px = 4000.;
    const double zoom = bar_flicks(4, 4) / bar_px;
    // The interval shown starts a quarter after the score's first bar line.
    const TimeVal delta{int64_t(ossia::quarter_duration<int64_t>)};
    const double width = 600.;
    const double bar3 = 2. * bar_px - delta.toPixels(zoom);
    const double left = bar3 - width / 3.;

    const auto lines = draw(measures, zoom, left, width, delta);
    CHECK(lines.has_bar_at(bar3));
    CHECK(grid_lines::between(lines.subs, left, bar3) > 0);
    CHECK(grid_lines::between(lines.subs, bar3, left + width) > 0);
  });
}

TEST_CASE(
    "Zoomed out, every bar line of the view is drawn up to its right edge",
    "[scenario][musical-grid]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Scenario::TimeSignatureMap measures;
    measures[TimeVal::zero()] = ossia::time_signature{4, 4};
    // A bar 150 pixels wide: the main lines are bars.
    const double bar_px = 150.;
    const double zoom = bar_flicks(4, 4) / bar_px;
    const double width = 1000.;
    // The view starts just before a bar line, so the first line drawn is the
    // one before, almost a bar left of the view.
    const double left = 10. * bar_px - 1.;

    const auto lines = draw(measures, zoom, left, width);
    for(double x = 10. * bar_px; x < left + width; x += bar_px)
    {
      INFO("bar line at " << x);
      CHECK(lines.has_bar_at(x));
    }
    CHECK(grid_lines::between(lines.subs, left + width - bar_px, left + width) > 0);
  });
}
