#include <Fx/Envelope.hpp>
#include <Fx/LFO_v2.hpp>
#include <Fx/MathAudioFilter.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <limits>
#include <numbers>
#include <vector>

using Catch::Approx;

namespace
{
// Wire a MathAudioFilter node to C input / output channel buffers.
struct filter_harness
{
  Nodes::MathAudioFilter::Node node;

  void
  wire(double** ins, double** outs, int channels, double rate = 48000., int frames = 16)
  {
    node.inputs.audio.samples = ins;
    node.inputs.audio.channels = channels;
    node.outputs.audio.samples = outs;
    node.outputs.audio.channels = channels;
    node.prepare(halp::setup{
        .input_channels = channels,
        .output_channels = channels,
        .frames = frames,
        .rate = rate});
  }

  void run(int frames)
  {
    halp::tick_flicks tk{};
    tk.frames = frames;
    node(tk);
  }
};
}


TEST_CASE("MathAudioFilter: gain expression should scale every sample", "[fx][audio][exprtk]")
{
  static constexpr int N = 8;
  std::array<double, N> in{1, 2, 3, 4, -1, -2, -3, -4};
  std::array<double, N> out{};
  double* ins[1]{in.data()};
  double* outs[1]{out.data()};

  filter_harness h;
  h.wire(ins, outs, 1);
  h.node.inputs.expr.value = "out[0] := x[0] * 0.5;";
  h.run(N);

  for(int i = 0; i < N; i++)
    CHECK(out[i] == in[i] * 0.5);
  // In particular:
  CHECK(out[0] == 0.5);
  CHECK(out[1] == 1.0);
  CHECK(out[4] == -0.5);
}

TEST_CASE("MathAudioFilter: the a/b/c params are visible in the expression", "[fx][audio][exprtk]")
{
  static constexpr int N = 4;
  std::array<double, N> in{1, 2, 3, 4};
  std::array<double, N> out{};
  double* ins[1]{in.data()};
  double* outs[1]{out.data()};

  filter_harness h;
  h.wire(ins, outs, 1);
  h.node.inputs.expr.value = "out[0] := x[0] * a + b;";
  h.node.inputs.a.value = 0.25f;
  h.node.inputs.b.value = 1.f;
  h.run(N);

  const auto f = [](double x) { return x * double(0.25f) + 1.; };
  for(int i = 0; i < N; i++)
    CHECK(out[i] == Approx(f(in[i])).epsilon(1e-12));
}

TEST_CASE("MathAudioFilter: 2-tap FIR y[n] = (x[n] + x[n-1])/2 impulse response", "[fx][audio][exprtk]")
{
  static constexpr int N = 6;
  std::array<double, N> in{1, 0, 0, 0, 0, 0}; // unit impulse
  std::array<double, N> out{};
  double* ins[1]{in.data()};
  double* outs[1]{out.data()};

  filter_harness h;
  h.wire(ins, outs, 1);
  h.node.inputs.expr.value = "out[0] := 0.5 * x[0] + 0.5 * px[0];";
  h.run(N);

  CHECK(out[0] == 0.5);
  CHECK(out[1] == 0.5);
  for(int i = 2; i < N; i++)
    CHECK(out[i] == 0.0);
}

TEST_CASE("MathAudioFilter: t and fs symbols hold sample index and sample rate", "[fx][audio][exprtk]")
{
  static constexpr int N = 5;
  std::array<double, N> in{};
  std::array<double, N> out{};
  double* ins[1]{in.data()};
  double* outs[1]{out.data()};

  filter_harness h;
  h.wire(ins, outs, 1, 44100.);
  h.node.inputs.expr.value = "out[0] := t + fs;";
  h.run(N);

  for(int i = 0; i < N; i++)
    CHECK(out[i] == 44100. + i);
}

TEST_CASE("MathAudioFilter: independent per-channel expressions", "[fx][audio][exprtk]")
{
  static constexpr int N = 4;
  std::array<double, N> l{1, 2, 3, 4};
  std::array<double, N> r{10, 20, 30, 40};
  std::array<double, N> ol{}, or_{};
  double* ins[2]{l.data(), r.data()};
  double* outs[2]{ol.data(), or_.data()};

  filter_harness h;
  h.wire(ins, outs, 2);
  h.node.inputs.expr.value = "out[0] := x[0] * 2; out[1] := x[1] * 3;";
  h.run(N);

  // Channels stay independent, every sample is processed with its own input.
  for(int i = 0; i < N; i++)
  {
    CHECK(ol[i] == l[i] * 2.);
    CHECK(or_[i] == r[i] * 3.);
  }
}

TEST_CASE("MathAudioFilter: edge cases stay safe", "[fx][audio][exprtk][fuzz]")
{
  filter_harness h;

  SECTION("zero channels: early return, no crash")
  {
    h.wire(nullptr, nullptr, 0);
    h.node.inputs.expr.value = "out[0] := x[0];";
    h.run(16);
    SUCCEED("no crash with 0 channels");
  }

  SECTION("zero-length buffer: no crash, no write")
  {
    std::array<double, 1> in{1};
    std::array<double, 1> out{-1};
    double* ins[1]{in.data()};
    double* outs[1]{out.data()};
    h.wire(ins, outs, 1);
    h.node.inputs.expr.value = "out[0] := x[0];";
    h.run(0);
    CHECK(out[0] == -1.); // untouched
  }

  SECTION("single sample")
  {
    std::array<double, 1> in{0.5};
    std::array<double, 1> out{};
    double* ins[1]{in.data()};
    double* outs[1]{out.data()};
    h.wire(ins, outs, 1);
    h.node.inputs.expr.value = "out[0] := x[0] * 4;";
    h.run(1);
    CHECK(out[0] == 2.0);
  }

  SECTION("NaN / infinity / denormal input does not crash")
  {
    std::array<double, 4> in{
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::denorm_min(), 1.};
    std::array<double, 4> out{};
    double* ins[1]{in.data()};
    double* outs[1]{out.data()};
    h.wire(ins, outs, 1);
    h.node.inputs.expr.value = "out[0] := x[0] * 0.5;";
    h.run(4);
    CHECK(std::isnan(out[0]));
    CHECK(std::isinf(out[1]));
    CHECK(out[1] > 0.);
    CHECK(out[2] == std::numeric_limits<double>::denorm_min() * 0.5);
    CHECK(out[3] == 0.5);
  }

  SECTION("invalid expression: node refuses to run, output untouched")
  {
    std::array<double, 2> in{1, 2};
    std::array<double, 2> out{-7, -7};
    double* ins[1]{in.data()};
    double* outs[1]{out.data()};
    h.wire(ins, outs, 1);
    h.node.inputs.expr.value = "this is not exprtk (";
    h.run(2);
    CHECK(out[0] == -7.);
    CHECK(out[1] == -7.);
  }
}

// ---------------------------------------------------------------------------

namespace
{
struct env_capture
{
  std::vector<float> values;
  int calls = 0;

  static void receive(void* self, Nodes::multichannel_output_type v)
  {
    auto& e = *static_cast<env_capture*>(self);
    e.calls++;
    e.values.clear();
    if(auto* f = ossia_variant_alias::get_if<float>(&v))
      e.values.push_back(*f);
    else if(auto* vec = ossia_variant_alias::get_if<Nodes::multichannel_output_vector>(&v))
      e.values.assign(vec->begin(), vec->end());
  }
};
}

TEST_CASE("Envelope: block peak and RMS values", "[fx][audio][envelope]")
{
  Nodes::Envelope::Node node;
  env_capture rms, peak;
  node.outputs.rms.call.context = &rms;
  node.outputs.rms.call.function = &env_capture::receive;
  node.outputs.peak.call.context = &peak;
  node.outputs.peak.call.function = &env_capture::receive;

  SECTION("mono: peak is the exact absolute maximum")
  {
    std::array<double, 4> in{0.25, -0.75, 0.5, 0.};
    double* ins[1]{in.data()};
    node.inputs.audio.samples = ins;
    node.inputs.audio.channels = 1;
    node(4);

    REQUIRE(peak.calls == 1);
    REQUIRE(peak.values.size() == 1);
    CHECK(peak.values[0] == 0.75f);
  }

  SECTION("mono: RMS of a constant block is the textbook value")
  {
    static constexpr int N = 16;
    std::array<double, N> in;
    in.fill(0.5);
    double* ins[1]{in.data()};
    node.inputs.audio.samples = ins;
    node.inputs.audio.channels = 1;
    node(N);

    REQUIRE(rms.calls == 1);
    REQUIRE(rms.values.size() == 1);
    CHECK(rms.values[0] == Approx(0.5).epsilon(1e-6));
  }

  SECTION("stereo: per-channel vectors, exact per-channel peaks")
  {
    std::array<double, 4> l{1., 0., 0., 0.};
    std::array<double, 4> r{0., -2., 0., 0.};
    double* ins[2]{l.data(), r.data()};
    node.inputs.audio.samples = ins;
    node.inputs.audio.channels = 2;
    node(4);

    REQUIRE(peak.values.size() == 2);
    CHECK(peak.values[0] == 1.f);
    CHECK(peak.values[1] == 2.f);

    REQUIRE(rms.values.size() == 2);
    // rms = sqrt(sum(x^2)/N): impulse of amplitude A over N samples -> A/sqrt(N)
    CHECK(rms.values[0] == Approx(1. / 2.).epsilon(1e-6));
    CHECK(rms.values[1] == Approx(2. / 2.).epsilon(1e-6));
  }

  SECTION("silence: exactly zero rms and peak")
  {
    std::array<double, 8> in{};
    double* ins[1]{in.data()};
    node.inputs.audio.samples = ins;
    node.inputs.audio.channels = 1;
    node(8);
    CHECK(rms.values.at(0) == 0.f);
    CHECK(peak.values.at(0) == 0.f);
  }

  SECTION("zero channels: no callback, no crash")
  {
    node.inputs.audio.samples = nullptr;
    node.inputs.audio.channels = 0;
    node(64);
    CHECK(rms.calls == 0);
    CHECK(peak.calls == 0);
  }

  SECTION("zero-length block: defined zero output")
  {
    std::array<double, 1> in{1.};
    double* ins[1]{in.data()};
    node.inputs.audio.samples = ins;
    node.inputs.audio.channels = 1;
    node(0);
    REQUIRE(rms.calls == 1);
    CHECK(rms.values.at(0) == 0.f);
    CHECK(peak.values.at(0) == 0.f);
  }
}

// ---------------------------------------------------------------------------

namespace
{
constexpr double flicks_per_second = 705600000.;

halp::tick_flicks make_flicks_tick(int64_t start, int64_t end, int frames)
{
  halp::tick_flicks tk{};
  tk.frames = frames;
  tk.start_in_flicks = start;
  tk.end_in_flicks = end;
  return tk;
}

Nodes::LFO::v2::Node make_lfo(float freq, float ampl, float offset, auto waveform)
{
  Nodes::LFO::v2::Node lfo;
  lfo.inputs.freq.value = freq;
  lfo.inputs.ampl.value = ampl;
  lfo.inputs.offset.value = offset;
  lfo.inputs.jitter.value = 0.f;
  lfo.inputs.phase.value = 0.f;
  lfo.inputs.quant.value = 0.f; // free-running (the quantifier defaults to 1/4!)
  lfo.inputs.waveform.value = waveform;
  return lfo;
}
}

TEST_CASE("LFO v2: deterministic waveform math (jitter = 0)", "[fx][lfo]")
{
  using W = Control::Widgets::Waveform;
  // 0.1 s per tick at 1 Hz -> phase delta = 0.2*pi per tick
  const int64_t dt = int64_t(flicks_per_second / 10);
  const double ph_delta = 0.1 * 1. * 2. * std::numbers::pi;

  SECTION("sine: first tick starts at phase 0, then accumulates ph_delta")
  {
    auto lfo = make_lfo(1.f, 2.f, 0.5f, W::Sin);

    lfo(make_flicks_tick(0, dt, 64));
    REQUIRE(lfo.outputs.out.value.has_value());
    CHECK(*lfo.outputs.out.value == Approx(0.5).margin(1e-6)); // 2*sin(0)+0.5

    lfo(make_flicks_tick(dt, 2 * dt, 64));
    CHECK(
        *lfo.outputs.out.value
        == Approx(2. * std::sin(ph_delta) + 0.5).margin(1e-5));

    lfo(make_flicks_tick(2 * dt, 3 * dt, 64));
    CHECK(
        *lfo.outputs.out.value
        == Approx(2. * std::sin(2 * ph_delta) + 0.5).margin(1e-5));
  }

  SECTION("the phase control offsets the oscillator phase directly (radians)")
  {
    auto lfo = make_lfo(1.f, 1.f, 0.f, W::Sin);
    lfo.inputs.phase.value = 0.25f;
    lfo(make_flicks_tick(0, dt, 64));
    CHECK(*lfo.outputs.out.value == Approx(std::sin(0.25)).margin(1e-6));
  }

  SECTION("square: sign of the sine, scaled by ampl and offset")
  {
    auto lfo = make_lfo(1.f, 0.5f, 0.25f, W::Square);
    // At phase 0: sin(0) = 0 -> not > 0 -> -1.
    lfo(make_flicks_tick(0, dt, 64));
    CHECK(*lfo.outputs.out.value == Approx(0.5 * -1. + 0.25).margin(1e-6));
    // At phase 0.2*pi: sin > 0 -> +1.
    lfo(make_flicks_tick(dt, 2 * dt, 64));
    CHECK(*lfo.outputs.out.value == Approx(0.5 * 1. + 0.25).margin(1e-6));
  }

  SECTION("saw: atan(tan(ph))/(pi/2) is linear in ph in ]-pi/2, pi/2[")
  {
    auto lfo = make_lfo(1.f, 1.f, 0.f, W::Saw);
    lfo(make_flicks_tick(0, dt, 64));   // ph = 0 -> 0
    CHECK(*lfo.outputs.out.value == Approx(0.).margin(1e-6));
    lfo(make_flicks_tick(dt, 2 * dt, 64)); // ph = 0.2*pi -> 0.4
    CHECK(*lfo.outputs.out.value == Approx(0.4).margin(1e-5));
  }

  SECTION("triangle: asin(sin(ph))/(pi/2), linear on the rising quarter-wave")
  {
    auto lfo = make_lfo(1.f, 1.f, 0.f, W::Triangle);
    lfo(make_flicks_tick(dt, 2 * dt, 64)); // first tick: ph = 0... but phase
    CHECK(*lfo.outputs.out.value == Approx(0.).margin(1e-6));
    lfo(make_flicks_tick(2 * dt, 3 * dt, 64)); // ph = 0.2*pi < pi/2 -> 0.4
    CHECK(*lfo.outputs.out.value == Approx(0.4).margin(1e-5));
  }

  SECTION("frequency scales the phase increment")
  {
    auto lfo = make_lfo(2.5f, 1.f, 0.f, W::Sin);
    lfo(make_flicks_tick(0, dt, 64));
    lfo(make_flicks_tick(dt, 2 * dt, 64));
    CHECK(
        *lfo.outputs.out.value
        == Approx(std::sin(2.5 * ph_delta)).margin(1e-5));
  }

  SECTION("zero-length tick: no phase advance")
  {
    auto lfo = make_lfo(1.f, 1.f, 0.f, W::Sin);
    lfo(make_flicks_tick(0, 0, 0));
    CHECK(*lfo.outputs.out.value == Approx(0.).margin(1e-9));
    lfo(make_flicks_tick(0, 0, 0));
    CHECK(*lfo.outputs.out.value == Approx(0.).margin(1e-9));
  }
}
