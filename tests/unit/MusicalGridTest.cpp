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
      // A bar many times wider than the view, the view centred on bar 3's line,
      // its finest sub-divisions narrower than half the view.
      for(const double bar_px : {1200., 4000., 16000.})
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

namespace
{
constexpr int64_t whole_note = 4 * ossia::quarter_duration<int64_t>;

struct signature
{
  int64_t start;
  int num, den;
};

struct model_bar
{
  int64_t start, end;
  int number;
  int num, den;
  int64_t length() const { return whole_note * num / den; }
};

//! A timeline of time signatures, and the bars it should show.
struct meter_map
{
  std::vector<signature> signatures;
  Scenario::TimeSignatureMap measures;
  std::vector<model_bar> bars;

  explicit meter_map(std::vector<signature> s, int64_t horizon)
      : signatures{std::move(s)}
  {
    for(auto& sig : signatures)
      measures[TimeVal{sig.start}]
          = ossia::time_signature{uint16_t(sig.num), uint16_t(sig.den)};

    int number = 0;
    for(std::size_t i = 0; i < signatures.size(); i++)
    {
      const auto& sig = signatures[i];
      const int64_t end = i + 1 < signatures.size() ? signatures[i + 1].start : horizon;
      const int64_t bar = whole_note * sig.num / sig.den;
      for(int64_t t = sig.start; t < end; t += bar)
        bars.push_back({t, std::min(t + bar, end), number++, sig.num, sig.den});
    }
  }

  //! Back to back signatures from `start`, each `bars_each` bars long, cycling
  //! through `cycle` up to `until`.
  static std::vector<signature> cycling(
      int64_t start, int64_t until, int bars_each,
      std::vector<std::pair<int, int>> cycle)
  {
    std::vector<signature> res;
    for(std::size_t i = 0; start < until; i++)
    {
      auto [num, den] = cycle[i % cycle.size()];
      res.push_back({start, num, den});
      start += bars_each * (whole_note * num / den);
    }
    return res;
  }

  const model_bar* bar_at(int64_t t) const
  {
    auto it = std::upper_bound(
        bars.begin(), bars.end(), t,
        [](int64_t t, const model_bar& b) { return t < b.start; });
    if(it == bars.begin())
      return nullptr;
    --it;
    return t < it->end ? &*it : nullptr;
  }
};

int64_t seconds(double s)
{
  return int64_t(s * ossia::flicks_per_second<double>);
}

struct drawn
{
  double left{}, width{};
  std::vector<double> bars, subs;
  //! The bar lines with their number on the ruler, in the pixels of the
  //! lines.
  std::vector<std::pair<double, int>> labels;
};

drawn draw_from(Scenario::MusicalGrid& grid, double zoom, TimeVal x0, double width)
{
  const double left = x0.toPixelsRaw(zoom);
  grid.compute(TimeVal::zero(), zoom, QRectF{left, 0., width, 100.}, x0);

  drawn res{left, width};
  for(auto& l : grid.timebars.lightBars->positions)
    res.bars.push_back(l.x1());
  for(auto& l : grid.timebars.lighterBars->positions)
    res.subs.push_back(l.x1());
  for(auto& m : grid.mainPositions)
    res.labels.push_back({m.pos_x + left + 100., m.timings.bars});
  return res;
}

//! The finest sub-division of a bar: its beat halved as long as there are
//! at most 64 in the bar.
int64_t finest_subdivision(int num, int den)
{
  int64_t per_beat = 1;
  while(num * per_beat * 2 <= 64)
    per_beat *= 2;
  return whole_note / (den * per_beat);
}

//! Checks what the grid drew against what the map holds.
void check_grid(const meter_map& map, double zoom, const drawn& d)
{
  constexpr double eps = 1e-6;
  const double right = d.left + d.width;
  auto px = [&](int64_t t) { return t / zoom; };
  // The map goes on past its last bar with the last signature: lines past
  // it are left out.
  auto in_view = [&](double x) {
    return x >= d.left - eps && x <= right + eps && x < px(map.bars.back().end);
  };
  INFO("zoom " << zoom << ", view from " << d.left << " px, " << d.width << " px wide");

  // Bounded by what the view can show.
  CHECK(d.bars.size() + d.subs.size() <= 2 * (std::size_t(d.width / 5.) + 4));

  // Bar lines: increasing, each on a bar of the map, labelled with its number.
  for(std::size_t i = 1; i < d.bars.size(); i++)
    REQUIRE(d.bars[i] - d.bars[i - 1] >= 5. - eps);
  for(auto [x, number] : d.labels)
  {
    if(!in_view(x))
      continue;
    INFO("bar line at " << x << " px, labelled " << number);
    const auto* bar = map.bar_at(int64_t(std::llround(x * zoom)));
    REQUIRE(bar);
    // Zoomed in far enough that every bar line is drawn, each is on its bar.
    if(zoom * 30. <= whole_note / 16.)
    {
      CHECK(std::abs(px(bar->start) - x) < 1e-3);
      CHECK(bar->number == number);
    }
  }

  // Zoomed in so that the shortest bar is 30 pixels wide, every bar line of
  // the view is drawn, once.
  if(zoom * 30. <= whole_note / 16.)
  {
    for(const auto& bar : map.bars)
    {
      if(!in_view(px(bar.start)))
        continue;
      INFO(
          "bar " << bar.number << " (" << bar.num << "/" << bar.den << ") at "
                 << px(bar.start));
      CHECK(std::count_if(d.bars.begin(), d.bars.end(), [&](double x) {
        return std::abs(x - px(bar.start)) < 1e-3;
      }) == 1);
    }
  }

  // Sub-divisions: strictly inside a bar, in steps of a power of two of its
  // beat, no finer than a 64th of it, evenly spaced from its bar line.
  for(double x : d.subs)
  {
    if(!in_view(x))
      continue;
    INFO("sub-division at " << x << " px");
    const auto* bar = map.bar_at(int64_t(std::llround(x * zoom)));
    REQUIRE(bar);
    CHECK(x > px(bar->start) + eps);
    CHECK(x < px(bar->end) - eps);

    // Its step: the gap to the next line of the same bar.
    auto next_sub = std::upper_bound(d.subs.begin(), d.subs.end(), x + eps);
    double next = px(bar->end);
    if(next_sub != d.subs.end() && *next_sub < next)
      next = *next_sub;
    if(next > right)
      continue;
    const double step_px = next - x;
    // The last one before a bar cut short by a signature change can be
    // closer to it.
    if(next == px(bar->end) && bar->end != bar->start + bar->length())
      continue;

    INFO("step " << step_px << " px in a bar of " << bar->num << "/" << bar->den);
    const double finest = finest_subdivision(bar->num, bar->den) / zoom;
    CHECK(step_px >= finest - 1e-3);
    const double halvings = std::log2((whole_note / double(bar->den)) / zoom / step_px);
    CHECK(std::abs(halvings - std::round(halvings)) < 1e-6);
    // On the grid of that step from the bar line: evenly spaced.
    const double from_bar = (x - px(bar->start)) / step_px;
    CHECK(std::abs(from_bar - std::round(from_bar)) < 0.5 / step_px);
  }
}

//! The ways the view meets a signature change: around it, starting on it,
//! ending on it.
std::vector<TimeVal> views_around(int64_t t, double zoom, double width)
{
  const int64_t w = int64_t(width * zoom);
  return {TimeVal{t - w / 2}, TimeVal{t}, TimeVal{t - w}, TimeVal{t - w / 3}};
}

void check_map_at_zooms(const meter_map& map, std::size_t max_changes = 12)
{
  Scenario::Timebars bars;
  std::unique_ptr<Scenario::LightBars> light{new Scenario::LightBars};
  std::unique_ptr<Scenario::LighterBars> lighter{new Scenario::LighterBars};
  bars.lightBars = light.get();
  bars.lighterBars = lighter.get();
  Scenario::MusicalGrid grid{bars};
  grid.setMeasures(map.measures);

  const double width = 1000.;
  // From the closest the timeline zooms to bars of a few pixels.
  for(const double zoom : {1000., 1e4, 1e5, 1e6, 3e6, 1e7, 1e8})
  {
    std::vector<int64_t> times;
    for(std::size_t i = 0; i < map.signatures.size() && i < max_changes; i++)
      times.push_back(map.signatures[i].start);
    for(std::size_t i
        = map.signatures.size() > max_changes ? map.signatures.size() - max_changes : 0;
        i < map.signatures.size(); i++)
      times.push_back(map.signatures[i].start);
    // Bar lines away from the changes.
    for(std::size_t i = 1; i < map.bars.size(); i += map.bars.size() / 7 + 1)
      times.push_back(map.bars[i].start);

    for(int64_t t : times)
      for(TimeVal x0 : views_around(t, zoom, width))
      {
        if(x0.impl < 0)
          continue;
        check_grid(map, zoom, draw_from(grid, zoom, x0, width));
      }
  }
}
}

TEST_CASE(
    "Odd meters: bar lines, numbers and even sub-divisions", "[scenario][musical-grid]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    for(auto [num, den] :
        {std::pair{4, 4}, std::pair{3, 4}, std::pair{5, 4}, std::pair{7, 8},
         std::pair{7, 16}, std::pair{9, 8}, std::pair{11, 8}, std::pair{13, 16},
         std::pair{15, 16}, std::pair{17, 16}, std::pair{21, 16}})
    {
      DYNAMIC_SECTION(num << "/" << den)
      {
        const meter_map map{{{0, num, den}}, seconds(4000)};
        check_map_at_zooms(map);
      }
    }
  });
}

TEST_CASE(
    "Mixed denominators back to back, two bars each, for forty minutes",
    "[scenario][musical-grid]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    const meter_map map{
        meter_map::cycling(
            0, seconds(2400), 2, {{7, 8}, {4, 4}, {5, 16}, {3, 2}, {6, 8}, {13, 16}}),
        seconds(3000)};
    check_map_at_zooms(map);
  });
}

TEST_CASE(
    "A signature change every bar, short bars next to long ones",
    "[scenario][musical-grid]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    const meter_map map{
        meter_map::cycling(
            0, seconds(2400), 1,
            {{7, 8},
             {5, 16},
             {11, 8},
             {2, 16},
             {12, 8},
             {1, 16},
             {4, 2},
             {13, 16},
             {9, 8}}),
        seconds(3000)};
    check_map_at_zooms(map);
  });
}

TEST_CASE(
    "Signature changes half an hour in, one of them inside a bar",
    "[scenario][musical-grid]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    // 4/4 up to 30 minutes, 7/16 from half a second later (a 4/4 bar cut
    // short), 21/16 from 45 minutes, 3/2 a 16th later.
    const meter_map map{
        {{0, 4, 4},
         {seconds(1800.5), 7, 16},
         {seconds(2700), 21, 16},
         {seconds(2700) + whole_note / 16, 3, 2}},
        seconds(4500)};
    check_map_at_zooms(map);
  });
}

TEST_CASE(
    "Zoomed in to the closest, sub-divisions are 64ths of the bar and the same width",
    "[scenario][musical-grid]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    for(auto [num, den] : {std::pair{4, 4}, std::pair{3, 4}, std::pair{7, 8}})
      for(const double at : {6., 1800., 3600.})
      {
        DYNAMIC_SECTION(num << "/" << den << ", " << at << " s in")
        {
          const meter_map map{{{0, num, den}}, seconds(3700)};
          // Around the bar line nearest to `at`, wide enough for a few 64ths.
          const auto* bar = map.bar_at(seconds(at));
          REQUIRE(bar);
          const double zoom = 1000.;
          const double step = finest_subdivision(num, den) / zoom;
          const double width = 8. * step;
          const auto d = [&] {
            Scenario::Timebars bars;
            std::unique_ptr<Scenario::LightBars> light{new Scenario::LightBars};
            std::unique_ptr<Scenario::LighterBars> lighter{new Scenario::LighterBars};
            bars.lightBars = light.get();
            bars.lighterBars = lighter.get();
            Scenario::MusicalGrid grid{bars};
            grid.setMeasures(map.measures);
            return draw_from(
                grid, zoom, TimeVal{bar->start - int64_t(width / 2 * zoom)}, width);
          }();

          // 4/4: a 64th of the bar. 3/4 and 7/8: the beat halved down to no
          // finer than that, so that the beats keep their lines.
          if(num == 4 && den == 4)
            CHECK(std::abs(step - bar_flicks(4, 4) / 64. / zoom) < 1e-6);
          CHECK(step >= bar_flicks(num, den) / 64. / zoom - 1e-6);
          CHECK(step < 2. * bar_flicks(num, den) / 64. / zoom);

          // Every line of the view, bar line included, the same step apart.
          std::vector<double> lines = d.subs;
          lines.insert(lines.end(), d.bars.begin(), d.bars.end());
          std::sort(lines.begin(), lines.end());
          std::erase_if(
              lines, [&](double x) { return x < d.left || x > d.left + width; });
          REQUIRE(lines.size() >= 6);
          for(std::size_t i = 1; i < lines.size(); i++)
          {
            INFO("gap " << i << ": " << lines[i] - lines[i - 1] << " px");
            CHECK(std::abs((lines[i] - lines[i - 1]) - step) < 0.5);
          }
        }
      }
  });
}

TEST_CASE(
    "Zoomed in near a bar line, its sub-divisions are the same width on both sides",
    "[scenario][musical-grid]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    // Bars of 740 to 1930 pixels: sub-divisions between 5 and 40 pixels that
    // do not fall on whole pixels.
    for(const double zoom : {1e6, 7.3e5, 1.9e6})
      for(const double at : {6., 1800., 3600.})
      {
        DYNAMIC_SECTION(zoom << " flicks per pixel, " << at << " s in")
        {
          const meter_map map{{{0, 4, 4}}, seconds(3700)};
          const auto* bar = map.bar_at(seconds(at));
          REQUIRE(bar);
          const double width = 1200.;
          Scenario::Timebars bars;
          std::unique_ptr<Scenario::LightBars> light{new Scenario::LightBars};
          std::unique_ptr<Scenario::LighterBars> lighter{new Scenario::LighterBars};
          bars.lightBars = light.get();
          bars.lighterBars = lighter.get();
          Scenario::MusicalGrid grid{bars};
          grid.setMeasures(map.measures);
          const auto d = draw_from(
              grid, zoom, TimeVal{bar->start - int64_t(width / 2 * zoom)}, width);

          std::vector<double> lines = d.subs;
          lines.insert(lines.end(), d.bars.begin(), d.bars.end());
          std::sort(lines.begin(), lines.end());
          std::erase_if(
              lines, [&](double x) { return x < d.left || x > d.left + width; });
          REQUIRE(lines.size() >= 10);
          const double step = lines[1] - lines[0];
          for(std::size_t i = 1; i < lines.size(); i++)
          {
            INFO(
                "gap " << i << ": " << lines[i] - lines[i - 1] << " px, first " << step);
            CHECK(std::abs((lines[i] - lines[i - 1]) - step) < 0.5);
          }
        }
      }
  });
}

TEST_CASE(
    "Odd meters: a signature changed in place shows at once", "[scenario][musical-grid]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    auto sigs
        = meter_map::cycling(0, seconds(600), 1, {{7, 8}, {5, 16}, {11, 8}, {13, 16}});
    meter_map map{sigs, seconds(700)};

    Scenario::Timebars bars;
    std::unique_ptr<Scenario::LightBars> light{new Scenario::LightBars};
    std::unique_ptr<Scenario::LighterBars> lighter{new Scenario::LighterBars};
    bars.lightBars = light.get();
    bars.lighterBars = lighter.get();
    Scenario::MusicalGrid grid{bars};
    // The grid reads the map it is given, changed in place below.
    Scenario::TimeSignatureMap measures = map.measures;
    grid.setMeasures(measures);

    const double zoom = 2e6;
    const TimeVal x0{sigs[40].start - int64_t(300 * zoom)};
    const auto before = draw_from(grid, zoom, x0, 1000.);
    check_grid(map, zoom, before);

    // The 13/16 of change 40 becomes 15/16, then 7/8 to the end.
    std::vector<signature> changed{sigs.begin(), sigs.begin() + 41};
    changed[40].num = 15;
    changed.push_back({changed[40].start + whole_note * 15 / 16, 7, 8});
    const meter_map map2{changed, seconds(700)};
    measures = map2.measures;

    const auto after = draw_from(grid, zoom, x0, 1000.);
    CHECK(after.bars != before.bars);
    check_grid(map2, zoom, after);
  });
}
