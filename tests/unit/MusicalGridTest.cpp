// The bar lines and their sub-divisions the full view draws for the part of
// the timeline it shows, zoomed in or out.

#include <Scenario/Document/Interval/FullView/TimeSignatureItem.hpp>
#include <Scenario/Document/Interval/FullView/Timebar.hpp>
#include <Scenario/Document/Interval/TimeSignatureMap.hpp>
#include <Scenario/Document/ScenarioDocument/MusicalGrid.hpp>

#include <ossia/editor/scenario/time_signature.hpp>

#include <QApplication>
#include <QElapsedTimer>

#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>
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

namespace
{
//! The lines of a view `width` pixels wide whose left edge is `seconds` into
//! the timeline.
grid_lines draw_at(
    const Scenario::TimeSignatureMap& measures, double zoom, double seconds,
    double width)
{
  const double left
      = TimeVal(int64_t(seconds * ossia::flicks_per_second<double>)).toPixels(zoom);
  return draw(measures, zoom, left, width);
}

//! A time signature change every few bars over an hour, as in an imported
//! MIDI file.
Scenario::TimeSignatureMap hour_of_signature_changes()
{
  Scenario::TimeSignatureMap measures;
  TimeVal t = TimeVal::zero();
  for(int i = 0; t.impl < 3600 * ossia::flicks_per_second<int64_t>; i++)
  {
    const auto sig = (i % 2) ? ossia::time_signature{3, 4} : ossia::time_signature{4, 4};
    measures[t] = sig;
    t.impl += 4 * int64_t(bar_flicks(sig.upper, sig.lower));
  }
  return measures;
}

//! What a view can show: a line every few pixels, and the one before it.
std::size_t max_lines(double width)
{
  return std::size_t(width / 5.) + 2;
}
}

TEST_CASE(
    "Zoomed in as far as the timeline goes, only the lines in the view are made",
    "[scenario][musical-grid]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Scenario::TimeSignatureMap measures;
    measures[TimeVal::zero()] = ossia::time_signature{4, 4};
    const auto changes = hour_of_signature_changes();

    // 1000 flicks per pixel is the closest the timeline zooms: a 4/4 bar is
    // then 1.4 million pixels wide.
    for(const double zoom : {100., 1000., 10000.})
      for(const double seconds : {0., 1.3, 1800.5})
        for(const double width : {800., 3840.})
        {
          DYNAMIC_SECTION(
              zoom << " flicks per pixel, " << seconds << " s, " << width << " px")
          {
            for(const Scenario::TimeSignatureMap* m :
                {&std::as_const(measures), &changes})
            {
              const auto lines = draw_at(*m, zoom, seconds, width);
              CHECK(lines.bars.size() <= 4);
              CHECK(lines.subs.size() <= max_lines(width));
              // Still every sub-division of the view.
              CHECK(lines.subs.size() >= std::size_t(width / 40.));
            }
          }
        }
  });
}

TEST_CASE(
    "Zoomed out on many time signature changes, the lines are a few pixels apart",
    "[scenario][musical-grid]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    const auto changes = hour_of_signature_changes();
    const double width = 1600.;
    const double hour = 3600. * ossia::flicks_per_second<double>;
    // From the whole hour in the view to a bar a pixel and a half wide.
    for(const double zoom : {1.1 * hour / width, 1e9, 3e8})
    {
      DYNAMIC_SECTION(zoom << " flicks per pixel")
      {
        const auto lines = draw_at(changes, zoom, 0., width);
        INFO(lines.bars.size() << " bar lines");
        CHECK(lines.bars.size() <= max_lines(width));
        CHECK(lines.bars.size() > 10);
        for(std::size_t i = 1; i < lines.bars.size(); i++)
        {
          INFO("line " << i << " at " << lines.bars[i]);
          REQUIRE(lines.bars[i] - lines.bars[i - 1] >= 5.);
        }
      }
    }
  });
}

TEST_CASE(
    "After a time signature change, its bar line is drawn once and counted from the "
    "start",
    "[scenario][musical-grid]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    // 3 bars of 4/4, 2 of 3/4, then 6/8 from half-way in a bar of 3/4.
    Scenario::TimeSignatureMap measures;
    const double b44 = bar_flicks(4, 4), b34 = bar_flicks(3, 4);
    measures[TimeVal::zero()] = ossia::time_signature{4, 4};
    measures[TimeVal(int64_t(3 * b44))] = ossia::time_signature{3, 4};
    const TimeVal six_eight{int64_t(3 * b44 + 2.5 * b34)};
    measures[six_eight] = ossia::time_signature{6, 8};

    Scenario::Timebars bars;
    std::unique_ptr<Scenario::LightBars> light{new Scenario::LightBars};
    std::unique_ptr<Scenario::LighterBars> lighter{new Scenario::LighterBars};
    bars.lightBars = light.get();
    bars.lighterBars = lighter.get();
    Scenario::MusicalGrid grid{bars};
    grid.setMeasures(measures);

    // A bar of 6/8 is 300 pixels wide: the main lines are bars.
    const double zoom = bar_flicks(6, 8) / 300.;
    const double left = six_eight.toPixels(zoom) - 50.;
    grid.compute(
        TimeVal::zero(), zoom, QRectF{left, 0., 1000., 100.},
        TimeVal::fromPixels(left, zoom));

    // The ruler's 0 is 100 pixels left of the view.
    const double first = six_eight.toPixels(zoom) - left - 100.;
    std::vector<int> counted;
    for(auto& m : grid.mainPositions)
      if(m.pos_x >= first - 0.5)
        counted.push_back(m.timings.bars);
    // The half bar of 3/4 is a bar too: the 6/8 starts at the 7th. The last
    // line is the one past the right of the view.
    CHECK(counted == std::vector<int>{6, 7, 8, 9, 10});
  });
}

TEST_CASE(
    "Asked again for the same view, the grid is not computed again",
    "[scenario][musical-grid]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Scenario::TimeSignatureMap measures;
    measures[TimeVal::zero()] = ossia::time_signature{4, 4};

    Scenario::Timebars bars;
    std::unique_ptr<Scenario::LightBars> light{new Scenario::LightBars};
    std::unique_ptr<Scenario::LighterBars> lighter{new Scenario::LighterBars};
    bars.lightBars = light.get();
    bars.lighterBars = lighter.get();
    Scenario::MusicalGrid grid{bars};
    grid.setMeasures(measures);

    // Whether the last call computed the lines: a computation fills them in
    // again.
    auto computed = [&] {
      const bool res = !light->positions.empty();
      light->positions.clear();
      return res;
    };

    const double zoom = bar_flicks(4, 4) / 150.;
    const QRectF view{0., 0., 1000., 100.};
    grid.compute(TimeVal::zero(), zoom, view, TimeVal::zero());
    CHECK(computed());
    grid.compute(TimeVal::zero(), zoom, view, TimeVal::zero());
    CHECK(!computed());

    // The same map, changed in place.
    measures[TimeVal(int64_t(2 * bar_flicks(4, 4)))] = ossia::time_signature{3, 4};
    grid.compute(TimeVal::zero(), zoom, view, TimeVal::zero());
    CHECK(computed());

    grid.compute(TimeVal::zero(), zoom / 2., view, TimeVal::zero());
    CHECK(computed());
    grid.compute(TimeVal::zero(), zoom / 2., view.translated(10., 0.), TimeVal::zero());
    CHECK(computed());
    grid.compute(
        TimeVal(int64_t(1000)), zoom / 2., view.translated(10., 0.), TimeVal::zero());
    CHECK(computed());
    grid.compute(
        TimeVal(int64_t(1000)), zoom / 2., view.translated(10., 0.),
        TimeVal::fromPixels(1., zoom));
    CHECK(computed());
  });
}

TEST_CASE(
    "Zoomed in on an hour of time signature changes, a view is computed quickly",
    "[scenario][musical-grid]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    const auto changes = hour_of_signature_changes();
    Scenario::Timebars bars;
    std::unique_ptr<Scenario::LightBars> light{new Scenario::LightBars};
    std::unique_ptr<Scenario::LighterBars> lighter{new Scenario::LighterBars};
    bars.lightBars = light.get();
    bars.lighterBars = lighter.get();
    Scenario::MusicalGrid grid{bars};
    grid.setMeasures(changes);

    // Scrolling a pixel at a time, at the closest zoom, half an hour in.
    const double zoom = 1000.;
    const double left
        = TimeVal(int64_t(1800 * ossia::flicks_per_second<double>)).toPixels(zoom);
    QElapsedTimer timer;
    timer.start();
    for(int i = 0; i < 50; i++)
      grid.compute(
          TimeVal::zero(), zoom, QRectF{left + i, 0., 1920., 100.},
          TimeVal::fromPixels(left + i, zoom));
    // Some tens of microseconds each, even unoptimised: a frame is 16 ms.
    CHECK(timer.elapsed() < 100);
  });
}
