// The ticks and labels of the time ruler at every zoom level, the thresholds
// between its graduations included.

#include <Scenario/Document/TimeRuler/TimeRuler.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include <limits>
#include <vector>

namespace
{
// Where the ruler changes graduations, in pixels per second.
const std::vector<double> thresholds{
    0.0125, 0.025,   0.05,    0.1,     0.2,      0.5,      1,       2,
    5,      10,      20,      40,      80,       100,      200,     500,
    1000,   2000,    5000,    10000,   20000,    50000,    100000,  200000,
    500000, 1000000, 2000000, 5000000, 10000000, 20000000, 50000000};

//! The smallest step a label in `format` spells, in milliseconds.
double labelUnit(Scenario::TimeRuler::Format format)
{
  switch(format)
  {
    case Scenario::TimeRuler::Format::Hours:
      return 60000.;
    case Scenario::TimeRuler::Format::Seconds:
      return 1000.;
    case Scenario::TimeRuler::Format::Milliseconds:
      return 1.;
    case Scenario::TimeRuler::Format::Microseconds:
      return 0.001;
  }
  return 0.;
}

void checkGraduations(double pps)
{
  INFO(pps << " pixels per second");
  const auto g = Scenario::timeRulerGraduations(pps);

  // A tick every few pixels, not one per pixel nor one per screen.
  CHECK(g.spacing >= 10.);
  if(pps <= thresholds.back())
    CHECK(g.spacing <= 50.);

  // The pixels between two ticks are the time between them at this zoom:
  // the labels say the time under them.
  CHECK(std::abs(g.spacing - pps * g.delta / 1000.) <= 1e-9 * g.spacing);

  // Consecutive labels spell different times.
  const double label_step = g.marksEvery * g.delta;
  const double unit = labelUnit(g.format);
  INFO("labels every " << label_step << " ms, spelled to " << unit << " ms");
  REQUIRE(label_step >= unit);
  const double units = label_step / unit;
  CHECK(std::abs(units - std::round(units)) < 1e-6);
}
}

TEST_CASE(
    "At and around every graduation threshold, the ruler has a graduation",
    "[scenario][time-ruler]")
{
  for(double t : thresholds)
  {
    for(double pps :
        {t, std::nextafter(t, 0.), std::nextafter(t, 1e300), t * (1. - 1e-9),
         t * (1. + 1e-9), t * 0.999, t * 1.001})
      checkGraduations(pps);
  }
}

TEST_CASE(
    "Zoomed further than its table, the ruler has a graduation",
    "[scenario][time-ruler]")
{
  // Days, weeks and years in the view, then closer than a nanosecond.
  for(double pps : {0.01, 1e-3, 1e-5, 1e-7, 1e8, 1e9, 1e10})
    checkGraduations(pps);
}

TEST_CASE(
    "Between thresholds, the ruler's graduations are the same", "[scenario][time-ruler]")
{
  // Some zooms in each range give the same tick duration as its threshold.
  for(std::size_t i = 0; i + 1 < thresholds.size(); i++)
  {
    INFO("from " << thresholds[i] << " to " << thresholds[i + 1]);
    const auto at = Scenario::timeRulerGraduations(thresholds[i]);
    for(double f : {0.25, 0.5, 0.75})
    {
      const double pps = thresholds[i] + f * (thresholds[i + 1] - thresholds[i]);
      CHECK(Scenario::timeRulerGraduations(pps).delta == at.delta);
    }
    CHECK(Scenario::timeRulerGraduations(thresholds[i + 1]).delta < at.delta);
  }
}

TEST_CASE("Without a usable zoom, the ruler has no ticks", "[scenario][time-ruler]")
{
  for(double pps :
      {0., -1., std::numeric_limits<double>::infinity(),
       std::numeric_limits<double>::quiet_NaN(),
       std::numeric_limits<double>::denorm_min()})
  {
    INFO(pps << " pixels per second");
    CHECK(Scenario::timeRulerGraduations(pps).spacing == 0.);
  }
}
