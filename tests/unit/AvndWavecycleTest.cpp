// ao::Wavecycle, driven as avnd's ossia binding drives it: the frequency
// reaches it through oscr::from_ossia_value, as a number or as a list / vec
// (one voice per value).

#include <avnd/binding/ossia/from_value.hpp>

#include <Advanced/Synth/Wavecycle.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

using Catch::Approx;
using WC = ao::Wavecycle;

namespace
{
constexpr double rate = 48000.;
constexpr int block = 64;

struct driver
{
  WC fx;
  int64_t position{};

  driver()
  {
    // A triangle: continuous, so that any jump in the output is the object's.
    fx.inputs.curve.value.push_back({0.f, 0.5f, [](float s) { return s; }});
    fx.inputs.curve.value.push_back({0.5f, 1.f, [](float s) { return 1.f - s; }});
    fx.prepare(halp::setup{
        .input_channels = 0, .output_channels = 1, .frames = block, .rate = rate});
  }

  void send(const ossia::value& v)
  {
    oscr::from_ossia_value(fx.inputs.frequency, v, fx.inputs.frequency.value);
  }

  std::vector<double> render(int frames)
  {
    std::vector<double> out;
    std::vector<double> buf(block);
    while(frames > 0)
    {
      const int n = std::min(frames, block);
      fx.outputs.audio.channel = buf.data();
      fx(halp::tick_musical{.frames = n, .position_in_frames = position});
      out.insert(out.end(), buf.begin(), buf.begin() + n);
      position += n;
      frames -= n;
    }
    return out;
  }
};

double max_step(const std::vector<double>& v)
{
  double m = 0.;
  for(std::size_t i = 1; i < v.size(); i++)
    m = std::max(m, std::abs(v[i] - v[i - 1]));
  return m;
}

//! Frequency from the upward zero crossings of a stretch of signal.
double measured_frequency(const std::vector<double>& v)
{
  int first = -1, last = -1, crossings = 0;
  for(std::size_t i = 1; i < v.size(); i++)
    if(v[i - 1] < 0. && v[i] >= 0.)
    {
      if(first < 0)
        first = int(i);
      last = int(i);
      crossings++;
    }
  if(crossings < 2)
    return 0.;
  return (crossings - 1) * rate / double(last - first);
}
}

TEST_CASE("Wavecycle: a frequency change bends the pitch without a jump", "[avnd][wavecycle]")
{
  driver d;
  d.send(1000.f);
  auto before = d.render(4800);
  d.send(1500.f);
  auto after = d.render(4800);

  before.insert(before.end(), after.begin(), after.end());
  // A unit triangle at 1500 Hz moves by 2 * 1500 / 48000 per sample at most.
  CHECK(max_step(before) < 2. * 1500. / rate * 1.05);
}

TEST_CASE("Wavecycle: plays the frequency asked, not a whole number of samples", "[avnd][wavecycle]")
{
  driver d;
  // 48000 / 1100 = 43.6 samples per cycle: truncated to 43, it played 1116 Hz.
  d.send(1100.f);
  d.render(4800);
  CHECK(measured_frequency(d.render(48000)) == Approx(1100.).epsilon(0.002));
}

TEST_CASE("Wavecycle: a list or a vec plays one voice per frequency", "[avnd][wavecycle]")
{
  auto solo = [](float f) {
    driver d;
    d.send(f);
    return d.render(9600);
  };
  const auto a = solo(200.f), b = solo(300.f), c = solo(500.f);

  for(const ossia::value& chord :
      {ossia::value{std::vector<ossia::value>{200.f, 300, 500.f}},
       ossia::value{ossia::vec3f{200.f, 300.f, 500.f}}})
  {
    driver d;
    d.send(chord);
    REQUIRE(d.fx.inputs.frequency.list.size() == 3);
    CHECK(d.fx.inputs.frequency.value == 200.f);
    const auto mix = d.render(9600);

    // Once the voices are in, the mix is their mean.
    double worst = 0.;
    for(int i = 4800; i < 9600; i++)
      worst = std::max(worst, std::abs(mix[i] - (a[i] + b[i] + c[i]) / 3.));
    CHECK(worst < 1e-4);
  }
}

TEST_CASE("Wavecycle: voices fade in and out as the list changes", "[avnd][wavecycle]")
{
  driver d;
  d.send(ossia::vec4f{200.f, 300.f, 400.f, 500.f});
  auto out = d.render(4800);
  d.send(250.f);
  auto later = d.render(4800);
  CHECK(d.fx.inputs.frequency.list.size() == 1);
  d.send(ossia::vec2f{250.f, 375.f});
  auto again = d.render(4800);

  out.insert(out.end(), later.begin(), later.end());
  out.insert(out.end(), again.begin(), again.end());
  // No click: the steps stay of the order of the voices' own slopes.
  CHECK(max_step(out) < 0.05);
}

TEST_CASE("Wavecycle: a list longer than the voices is cut, never grown", "[avnd][wavecycle]")
{
  driver d;
  std::vector<ossia::value> many(200, ossia::value{100.f});
  d.send(ossia::value{many});
  CHECK(d.fx.inputs.frequency.list.size() == std::size_t(WC::max_voices));
  CHECK(d.fx.inputs.frequency.list.capacity() == std::size_t(WC::max_voices));
}
