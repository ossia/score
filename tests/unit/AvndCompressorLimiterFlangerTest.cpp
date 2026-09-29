// Compressor v2, Limiter v1/v2 and Flanger v2 driven directly, with the values a
// cable or a synced time chooser can hand them.

#include <Advanced/Audio/Dynamics.hpp>
#include <Advanced/Audio/Flanger.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include <algorithm>
#include <limits>
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
  fx.prepare(
      {.input_channels = 1, .output_channels = 1, .frames = frames, .rate = rate});
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

template <typename Fx>
void prepare_at(Fx& fx, int frames, double sample_rate, int channels = 1)
{
  fx.prepare(
      {.input_channels = channels,
       .output_channels = channels,
       .frames = frames,
       .rate = sample_rate});
}

std::vector<double> sine(int frames, double sample_rate, double freq, double amp)
{
  std::vector<double> v(frames);
  for(int i = 0; i < frames; i++)
    v[i] = amp * std::sin(2. * std::numbers::pi * freq * i / sample_rate);
  return v;
}

//! Fraction of the samples from `from` on that sit on the ceiling: what a
//! clipper produces, not a gain riding the level
double on_ceiling(const std::vector<double>& v, double ceiling, int from)
{
  int n = 0;
  for(std::size_t i = from; i < v.size(); i++)
    n += std::abs(v[i]) >= 0.999 * ceiling;
  return double(n) / double(v.size() - from);
}

double mean(const std::vector<double>& v, int from)
{
  double s = 0.;
  for(std::size_t i = from; i < v.size(); i++)
    s += v[i];
  return s / double(v.size() - from);
}

int lookahead_samples(float lookahead, double sample_rate)
{
  return int(std::lround(double(lookahead) * sample_rate));
}
}

TEMPLATE_TEST_CASE(
    "Limiter: loud sines come out at the ceiling, not clipped", "[avnd][audio][limiter]",
    ao::Limiter, ao::Limiter_v2)
{
  for(double sr : {22050., 44100., 48000., 96000., 192000.})
    for(double t : {0.1, 0.5, 0.98})
      for(float makeup : {0.f, 30.f})
        for(double freq : {50., 440., 5000.})
        {
          CAPTURE(sr, t, makeup, freq);
          TestType fx;
          const int frames = int(sr);
          prepare_at(fx, frames, sr);
          fx.inputs.threshold.value = float(t);
          fx.inputs.makeup.value = makeup;
          const auto out = run_mono(fx, sine(frames, sr, freq, 1.));
          const double ceiling = double(float(t));
          const int settled = frames / 2;

          CHECK(peak(out) <= ceiling);
          // The gain holds the level: the waveform keeps its shape near the
          // ceiling instead of being flattened on it
          CHECK(peak(out, settled) >= 0.9 * ceiling);
          CHECK(on_ceiling(out, ceiling, settled) < 0.1);
        }
}

TEMPLATE_TEST_CASE(
    "Limiter: impulses never exceed the ceiling and the gain ramps before them",
    "[avnd][audio][limiter]", ao::Limiter, ao::Limiter_v2)
{
  for(double sr : {44100., 48000., 96000.})
  {
    CAPTURE(sr);
    TestType fx;
    const int frames = int(sr / 2);
    prepare_at(fx, frames, sr);
    fx.inputs.threshold.value = 0.5f;
    // A ramp shorter than the 32 samples between spikes, so the gain on the
    // first spike is not already heading for the next one.
    fx.inputs.attack.value = 0.0001f;
    const int L = lookahead_samples(fx.inputs.lookahead.value, sr);

    auto in = sine(frames, sr, 100., 0.3);
    const int k = frames / 3;
    for(int i = 0; i < 16; i++)
      in[k + 32 * i] += (i % 2 ? -10. : 10.);

    const auto out = run_mono(fx, in);
    CHECK(peak(out) <= 0.5);
    // The first transient is brought exactly to the ceiling by the gain...
    CHECK(std::abs(out[k + L]) == Approx(0.5).epsilon(1e-9));
    // ... which was already down on the sample before it
    CHECK(std::abs(out[k + L - 1]) < 0.5 * std::abs(in[k - 1]));
  }
}

TEMPLATE_TEST_CASE(
    "Limiter: transparent below the threshold, delayed by the lookahead",
    "[avnd][audio][limiter]", ao::Limiter, ao::Limiter_v2)
{
  for(double sr : {44100., 48000., 96000.})
    for(double t : {0.1, 0.5, 0.98})
    {
      CAPTURE(sr, t);
      TestType fx;
      const int frames = int(sr / 4);
      prepare_at(fx, frames, sr);
      fx.inputs.threshold.value = float(t);
      const int L = lookahead_samples(fx.inputs.lookahead.value, sr);

      // Just under the ceiling: no knee eats into it
      const auto in = sine(frames, sr, 440., 0.95 * t);
      const auto out = run_mono(fx, in);
      for(int i = 0; i < L; i++)
        REQUIRE(out[i] == 0.);
      double err = 0.;
      for(int i = L; i < frames; i++)
        err = std::max(err, std::abs(out[i] - in[i - L]));
      CHECK(err == 0.);
    }
}

TEMPLATE_TEST_CASE(
    "Limiter: Makeup is the input gain into the limiter", "[avnd][audio][limiter]",
    ao::Limiter, ao::Limiter_v2)
{
  const double sr = 48000.;
  const int frames = 24000;
  {
    TestType fx;
    prepare_at(fx, frames, sr);
    fx.inputs.makeup.value = 3.f; // x4, 0.4 stays under the default ceiling
    const auto out = run_mono(fx, sine(frames, sr, 440., 0.1));
    CHECK(peak(out, frames / 2) == Approx(0.4).epsilon(1e-3));
  }
  {
    TestType fx;
    prepare_at(fx, frames, sr);
    fx.inputs.makeup.value = 30.f;
    fx.inputs.threshold.value = 0.1f;
    const auto out = run_mono(fx, sine(frames, sr, 440., 0.1));
    CHECK(peak(out) <= double(0.1f));
    CHECK(peak(out, frames / 2) >= 0.09);
  }
}

TEMPLATE_TEST_CASE(
    "Limiter: no DC offset", "[avnd][audio][limiter]", ao::Limiter, ao::Limiter_v2)
{
  const double sr = 48000.;
  const int frames = 48000;
  {
    TestType fx;
    prepare_at(fx, 256, sr);
    CHECK(peak(run_mono(fx, std::vector<double>(256, 0.))) == 0.);
  }
  for(double t : {0.1, 0.98})
    for(float makeup : {0.f, 30.f})
    {
      CAPTURE(t, makeup);
      TestType fx;
      prepare_at(fx, frames, sr);
      fx.inputs.threshold.value = float(t);
      fx.inputs.makeup.value = makeup;
      // 100 Hz: the second half holds exactly 50 periods
      const auto out = run_mono(fx, sine(frames, sr, 100., 1.));
      CHECK(std::abs(mean(out, frames / 2)) < 1e-6);
    }
}

TEMPLATE_TEST_CASE(
    "Limiter: channels share the gain, the sidechain adds to it",
    "[avnd][audio][limiter]", ao::Limiter, ao::Limiter_v2)
{
  const double sr = 48000.;
  const int frames = 24000;
  TestType fx;
  prepare_at(fx, frames, sr, 3);
  fx.inputs.threshold.value = 0.5f;
  const int L = lookahead_samples(fx.inputs.lookahead.value, sr);

  auto loud = sine(frames, sr, 440., 2.);
  auto quiet = sine(frames, sr, 440., 0.2);
  std::vector<double> key(frames, 0.);
  std::vector<double> out_l(frames), out_r(frames);
  double* in_ptr[2]{loud.data(), quiet.data()};
  double* sc_ptr[1]{key.data()};
  double* out_ptr[2]{out_l.data(), out_r.data()};
  fx.inputs.audio.samples = in_ptr;
  fx.inputs.audio.channels = 2;
  fx.inputs.sidechain.samples = sc_ptr;
  fx.inputs.sidechain.channels = 1;
  fx.outputs.audio.samples = out_ptr;
  fx.outputs.audio.channels = 2;
  fx(frames);

  for(int i = frames / 2; i < frames; i++)
    if(std::abs(quiet[i - L]) > 0.05)
      REQUIRE(out_r[i] / quiet[i - L] == Approx(out_l[i] / loud[i - L]));
  CHECK(peak(out_l) <= 0.5);
  CHECK(peak(out_r, frames / 2) == Approx(0.05).epsilon(0.02));

  // A key louder than the input: gain ceiling / key
  std::ranges::fill(key, 5.);
  std::ranges::copy(quiet, loud.begin());
  fx(frames);
  CHECK(peak(out_l, frames / 2) == Approx(0.2 * 0.5 / 5.).epsilon(1e-3));
}

TEMPLATE_TEST_CASE(
    "Limiter: non-finite input and controls", "[avnd][audio][limiter]", ao::Limiter,
    ao::Limiter_v2)
{
  const double sr = 48000.;
  const int frames = 4800;
  constexpr double inf = std::numeric_limits<double>::infinity();
  constexpr double nan = std::numeric_limits<double>::quiet_NaN();

  auto all_finite = [](const std::vector<double>& v) {
    return std::ranges::all_of(v, [](double x) { return std::isfinite(x); });
  };

  {
    TestType fx;
    prepare_at(fx, frames, sr);
    fx.inputs.threshold.value = 0.5f;
    auto in = sine(frames, sr, 440., 1.);
    in[100] = nan;
    in[200] = inf;
    in[300] = -inf;
    const auto out = run_mono(fx, in);
    CHECK(all_finite(out));
    CHECK(peak(out) <= 0.5);
    // The state is not poisoned: the next buffer is still limited, not muted
    const auto next = run_mono(fx, sine(frames, sr, 440., 1.));
    CHECK(peak(next) <= 0.5);
    CHECK(peak(next) >= 0.45);
  }

  for(float bad : {float(nan), float(inf), -float(inf)})
  {
    CAPTURE(bad);
    auto run_with = [&](auto set) {
      TestType fx;
      prepare_at(fx, frames, sr);
      set(fx.inputs);
      return run_mono(fx, sine(frames, sr, 440., 1.));
    };
    CHECK(all_finite(run_with([&](auto& in) { in.threshold.value = bad; })));
    CHECK(all_finite(run_with([&](auto& in) { in.makeup.value = bad; })));
    CHECK(all_finite(run_with([&](auto& in) { in.attack.value = bad; })));
    CHECK(all_finite(run_with([&](auto& in) { in.release.value = bad; })));
    CHECK(all_finite(run_with([&](auto& in) { in.lookahead.value = bad; })));
  }
}

TEMPLATE_TEST_CASE(
    "Limiter: lookahead and attack changes keep the ceiling", "[avnd][audio][limiter]",
    ao::Limiter, ao::Limiter_v2)
{
  const double sr = 48000.;
  const int block = 64;
  TestType fx;
  prepare_at(fx, block, sr);
  fx.inputs.threshold.value = 0.5f;
  fx.inputs.makeup.value = 3.f;
  const auto in = sine(block * 400, sr, 440., 1.);
  std::vector<double> out;
  for(int b = 0; b < 400; b++)
  {
    fx.inputs.lookahead.value = (b % 7) * 0.001f;
    fx.inputs.attack.value = (b % 5) * 0.001f;
    auto chunk = run_mono(
        fx, std::vector<double>(in.begin() + b * block, in.begin() + (b + 1) * block));
    out.insert(out.end(), chunk.begin(), chunk.end());
  }
  CHECK(peak(out) <= 0.5);
  CHECK(on_ceiling(out, 0.5, 0) < 0.1);
}

TEST_CASE(
    "Compressor v2: a ratio of 0 from a cable is no division by zero",
    "[avnd][audio][compressor]")
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

TEST_CASE(
    "Compressor v2: the lookahead stays inside the delay line",
    "[avnd][audio][compressor]")
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

TEST_CASE(
    "Compressor v2: a buffer-size change restarts from silence",
    "[avnd][audio][compressor]")
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

TEST_CASE(
    "Flanger v2: Delay and Amount at their maximum fit in the comb",
    "[avnd][audio][flanger]")
{
  ao::Flanger_v2::inputs in;
  constexpr auto delay_range = decltype(in.delay)::range{};
  constexpr auto amount_range = decltype(in.amount)::range{};
  STATIC_CHECK(delay_range.max + amount_range.max < ao::Flanger_v2::max_delay);
}
