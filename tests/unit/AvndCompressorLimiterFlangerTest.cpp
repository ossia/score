// Compressor v2, Limiter v2 and Flanger v2 driven directly, with the values a
// cable or a synced time chooser can hand them.

#include <Advanced/Audio/Dynamics.hpp>
#include <Advanced/Audio/Flanger.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

using Catch::Approx;

namespace
{
constexpr double rate = 48000.;

//! One mono buffer through a Compressor_v2 / Limiter_v2
template <typename Fx>
std::vector<double> run_mono(Fx& fx, std::vector<double> in)
{
  const int frames = int(in.size());
  std::vector<double> out(in.size());
  double* in_ptr[1]{in.data()};
  double* out_ptr[1]{out.data()};
  fx.inputs.audio.samples = in_ptr;
  fx.inputs.audio.channels = 1;
  fx.inputs.sidechain.samples = nullptr;
  fx.inputs.sidechain.channels = 0;
  fx.outputs.audio.samples = out_ptr;
  fx.outputs.audio.channels = 1;
  fx(frames);
  return out;
}

template <typename Fx>
void prepare(Fx& fx, int frames)
{
  fx.prepare({.input_channels = 1, .output_channels = 1, .frames = frames, .rate = rate});
}

std::vector<double> impulse(int frames)
{
  std::vector<double> v(frames);
  v[0] = 1.;
  return v;
}

int argmax_abs(const std::vector<double>& v)
{
  return int(std::distance(
      v.begin(), std::max_element(v.begin(), v.end(), [](double a, double b) {
    return std::abs(a) < std::abs(b);
  })));
}

double peak(const std::vector<double>& v, int from = 0)
{
  double p = 0.;
  for(std::size_t i = from; i < v.size(); i++)
    p = std::max(p, std::abs(v[i]));
  return p;
}
}

TEST_CASE("Limiter v2: the threshold is the output ceiling", "[avnd][audio][limiter]")
{
  ao::Limiter_v2 fx;
  const int frames = 4800;
  prepare(fx, frames);

  std::vector<double> sine(frames);
  for(int i = 0; i < frames; i++)
    sine[i] = std::sin(2. * std::numbers::pi * 440. * i / rate);

  // Default threshold 0.98: a full-scale sine comes out just under it, not
  // squashed to a few hundredths.
  const auto out = run_mono(fx, sine);
  const double p = peak(out, frames / 2);
  CHECK(p <= 0.98);
  CHECK(p > 0.8);

  for(double t : {0.1, 0.5, 0.98})
  {
    ao::Limiter_v2 l;
    prepare(l, frames);
    l.inputs.threshold.value = float(t);
    l.inputs.makeup.value = 30.f; // 31x: the soft clip has to catch it
    CHECK(peak(run_mono(l, sine)) <= double(float(t)));
  }
}

TEST_CASE("Limiter v2: silence stays silent", "[avnd][audio][limiter]")
{
  ao::Limiter_v2 fx;
  prepare(fx, 256);
  const auto out = run_mono(fx, std::vector<double>(256, 0.));
  CHECK(peak(out) == 0.);
}

TEST_CASE("Compressor v2: a ratio of 0 from a cable is no division by zero", "[avnd][audio][compressor]")
{
  ao::Compressor_v2 fx;
  const int frames = 2048;
  prepare(fx, frames);
  fx.inputs.ratio.value = 0.f;
  fx.inputs.threshold.value = 0.1f;
  const auto out = run_mono(fx, std::vector<double>(frames, 0.9));
  CHECK(std::ranges::all_of(out, [](double x) { return std::isfinite(x); }));

  // The lowest ratio of the control instead: 0.05, an upward expansion
  ao::Compressor_v2 ref;
  prepare(ref, frames);
  ref.inputs.ratio.value = 0.05f;
  ref.inputs.threshold.value = 0.1f;
  CHECK(out == run_mono(ref, std::vector<double>(frames, 0.9)));
}

TEST_CASE("Compressor v2: the lookahead stays inside the delay line", "[avnd][audio][compressor]")
{
  // Ratio 1 and threshold 1: a pure delay by the lookahead
  auto delay_of = [](float lookahead) {
    ao::Compressor_v2 fx;
    const int frames = 16384;
    prepare(fx, frames);
    fx.inputs.ratio.value = 1.f;
    fx.inputs.threshold.value = 1.f;
    fx.inputs.lookahead.value = lookahead;
    return argmax_abs(run_mono(fx, impulse(frames)));
  };

  // The line is read after the sample is written: 1 ms is 47 samples late
  CHECK(delay_of(0.001f) == 47);
  // 0 would read the oldest sample of the line: one delay-line length late
  CHECK(delay_of(0.f) == 0);
  // Beyond the line, the read position would wrap around it
  CHECK(delay_of(1.f) <= int(ao::DynamicsProcessor::max_lookahead * rate));
  CHECK(delay_of(1.f) >= int(ao::DynamicsProcessor::max_lookahead * rate) - 2);
}

TEST_CASE("Compressor v2: a buffer-size change restarts from silence", "[avnd][audio][compressor]")
{
  ao::Compressor_v2 fx;
  prepare(fx, 64);
  fx.inputs.ratio.value = 1.f;
  fx.inputs.threshold.value = 1.f;
  fx.inputs.lookahead.value = 0.001f;
  run_mono(fx, std::vector<double>(64, 0.5));

  // The binding re-runs prepare() when the buffer grows: the line is emptied,
  // so the next buffer starts from silence.
  prepare(fx, 128);
  const auto out = run_mono(fx, std::vector<double>(128, 0.));
  CHECK(peak(out) == 0.);
}

TEST_CASE("Flanger v2: the delay stays inside the comb", "[avnd][audio][flanger]")
{
  auto echo_at = [](float delay) {
    ao::Flanger_v2 fx;
    fx.prepare({.input_channels = 1, .output_channels = 1, .frames = 1, .rate = rate});
    ao::Flanger_v2::inputs in;
    ao::Flanger_v2::outputs out;
    in.delay.value = delay;
    in.amount.value = 0.f;
    in.ffd.value = 0.f;
    in.fbk.value = 0.f;

    std::vector<double> res(8192);
    for(std::size_t i = 0; i < res.size(); i++)
      res[i] = fx(i == 0 ? 1. : 0., in, out);
    return argmax_abs(res);
  };

  // The all-pass interpolation adds a sample: 2 ms is 97 samples
  CHECK(echo_at(0.002f) == 97);
  // A delay of 0 would read the oldest sample: a whole line late
  CHECK(echo_at(0.f) == 3);
  // A synced note value can be seconds long: clamped to the line, not wrapped
  const int longest = int(ao::Flanger_v2::max_delay * rate);
  CHECK(echo_at(1.f) <= longest + 1);
  CHECK(echo_at(1.f) >= longest - 2);
}

TEST_CASE("Flanger v2: Delay and Amount at their maximum fit in the comb", "[avnd][audio][flanger]")
{
  ao::Flanger_v2::inputs in;
  constexpr auto delay_range = decltype(in.delay)::range{};
  constexpr auto amount_range = decltype(in.amount)::range{};
  STATIC_CHECK(delay_range.max + amount_range.max < ao::Flanger_v2::max_delay);
}
