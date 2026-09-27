#include <Fx/Envelope.hpp>
#include <Fx/LFO_v2.hpp>
#include <Fx/LFO_v3.hpp>
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

TEST_CASE("LFO v3: a period in seconds, or synced to the bars", "[fx][lfo]")
{
  auto make = [](float period, bool sync) {
    Nodes::LFO::v3::Node lfo;
    lfo.inputs.period.value = period;
    lfo.inputs.period.sync = sync;
    lfo.inputs.ampl.value = 1.f;
    lfo.inputs.offset.value = 0.f;
    lfo.inputs.jitter.value = 0.f;
    lfo.inputs.phase.value = 0.f;
    lfo.inputs.waveform.value = Nodes::LFO::v3::Sin;
    return lfo;
  };

  SECTION("free: a 1 s period is v2 at 1 Hz")
  {
    const int64_t dt = int64_t(flicks_per_second / 10);
    auto v3 = make(1.f, false);
    auto v2 = make_lfo(1.f, 1.f, 0.f, Control::Widgets::Waveform::Sin);
    for(int i = 0; i < 12; i++)
    {
      v3(make_flicks_tick(i * dt, (i + 1) * dt, 64));
      v2(make_flicks_tick(i * dt, (i + 1) * dt, 64));
      REQUIRE(v3.outputs.out.value.has_value());
      CHECK(*v3.outputs.out.value == Approx(*v2.outputs.out.value).margin(1e-6));
    }
  }

  SECTION("synced: a quarter note is one cycle per quarter, whatever the tempo")
  {
    for(double tempo : {60., 120., 173.})
    {
      // What the binding hands the object for a synced quarter: seconds.
      auto lfo = make(float(60. / tempo), true);
      auto tick = [&](double q0, double q1) {
        halp::tick_flicks tk{};
        tk.frames = 64;
        tk.tempo = tempo;
        tk.signature = {4, 4};
        tk.start_position_in_quarters = q0;
        tk.end_position_in_quarters = q1;
        tk.last_signature_change = 0.;
        lfo(tk);
        return *lfo.outputs.out.value;
      };
      tick(0., 0.25); // phase 0
      // A quarter of a quarter later: a quarter of a cycle, sin = 1
      CHECK(tick(0.25, 0.5) == Approx(1.).margin(1e-5));
      CHECK(tick(0.5, 0.75) == Approx(0.).margin(1e-5));
      CHECK(tick(0.75, 1.) == Approx(-1.).margin(1e-5));
    }
  }
}

TEST_CASE("LFO v3: shape, retrigger, the stepped modes and drift", "[fx][lfo]")
{
  using namespace Nodes::LFO::v3;
  auto make = [](Waveform w, float shape = 0.5f) {
    Node lfo;
    lfo.inputs.period.value = 1.f; // a cycle a second
    lfo.inputs.period.sync = false;
    lfo.inputs.shape.value = shape;
    lfo.inputs.ampl.value = 1.f;
    lfo.inputs.offset.value = 0.f;
    lfo.inputs.jitter.value = 0.f;
    lfo.inputs.phase.value = 0.f;
    lfo.inputs.waveform.value = w;
    return lfo;
  };
  // 100 ticks of 10 ms: one cycle
  const int64_t dt = int64_t(flicks_per_second / 100);
  auto run = [&](Node& lfo, int n, int64_t& t) {
    std::vector<std::optional<float>> out;
    for(int i = 0; i < n; i++, t += dt)
    {
      lfo.outputs.out.value.reset();
      lfo(make_flicks_tick(t, t + dt, 64));
      out.push_back(lfo.outputs.out.value);
    }
    return out;
  };

  SECTION("shape is the square's pulse width")
  {
    for(float shape : {0.25f, 0.5f, 0.8f})
    {
      auto lfo = make(Square, shape);
      int64_t t = 0;
      int high = 0;
      for(auto v : run(lfo, 100, t))
        high += *v > 0.f;
      CHECK(std::abs(high - int(shape * 100)) <= 2);
    }
  }

  SECTION("square on change: a value at each edge only; square: every tick")
  {
    auto on_change = make(SquareOnChange);
    auto every = make(Square);
    int64_t t1 = 0, t2 = 0;
    const auto a = run(on_change, 200, t1);
    const auto b = run(every, 200, t2);
    int sent_a = 0, sent_b = 0;
    for(auto& v : a)
      sent_a += v.has_value();
    for(auto& v : b)
      sent_b += v.has_value();
    CHECK(sent_b == 200);
    CHECK(sent_a >= 4); // the first, and two edges a cycle
    CHECK(sent_a <= 6);
  }

  SECTION("sample and hold: on change or every tick, the same held values")
  {
    auto on_change = make(SampleAndHold);
    auto every = make(SampleAndHoldEveryTick);
    int64_t t1 = 0, t2 = 0;
    const auto a = run(on_change, 200, t1);
    const auto b = run(every, 200, t2);
    int sent_a = 0;
    for(auto& v : a)
      sent_a += v.has_value();
    CHECK(sent_a >= 3);
    CHECK(sent_a <= 6);
    for(auto& v : b)
      REQUIRE(v.has_value());
    // Every tick repeats the held value between two changes
    int changes = 0;
    for(std::size_t i = 1; i < b.size(); i++)
      changes += *b[i] != *b[i - 1];
    CHECK(changes <= 5);
  }

  SECTION("0.5 is the plain waveform; sine and triangle peak where the shape says")
  {
    using N = Nodes::LFO::v3::Node;
    for(double x : {0., 0.1, 0.3, 0.6, 0.9})
    {
      CHECK(N::sine(x, 0.5) == Approx(std::sin(2. * std::numbers::pi * x)).margin(1e-9));
      CHECK(N::triangle(x, 0.5)
            == Approx(std::asin(std::sin(2. * std::numbers::pi * x)) * 2. / std::numbers::pi)
                   .margin(1e-9));
    }
    // A shape of 0.8: 80 % of the cycle rising from the trough (at -0.25) to
    // the peak, which is then at 0.55
    CHECK(N::sine(0.55, 0.8) == Approx(1.));
    CHECK(N::triangle(0.55, 0.8) == Approx(1.));
    CHECK(N::sine(0.75, 0.8) < 0.9);
    // Smooth: no kink, the slope changes gradually around the zero crossing
    double worst = 0.;
    const double h = 1e-3;
    for(double x = 0.; x < 1.; x += h)
    {
      const double d2 = N::sine(x + h, 0.8) - 2. * N::sine(x, 0.8) + N::sine(x - h, 0.8);
      worst = std::max(worst, std::abs(d2));
    }
    CHECK(worst < 1e-3);
  }

  SECTION("saw: one ramp a cycle, bent by the shape")
  {
    using N = Nodes::LFO::v3::Node;
    int drops = 0;
    for(double x = 0.; x < 1.; x += 0.01)
      drops += N::saw(x + 0.01, 0.5) < N::saw(x, 0.5);
    CHECK(drops == 1);
    CHECK(N::saw(0., 0.5) == Approx(0.).margin(1e-9));
    CHECK(N::saw(0.25, 0.5) == Approx(0.5));
    CHECK(N::saw(0.25, 0.2) < 0.5); // bent down
    CHECK(N::saw(0.25, 0.8) > 0.5); // bent up
  }

  SECTION("ramp down: the saw the other way")
  {
    auto lfo = make(RampDown);
    int64_t t = 0;
    const auto v = run(lfo, 100, t);
    CHECK(*v[0] == Approx(0.).margin(1e-6));
    CHECK(*v[25] == Approx(-0.5).margin(1e-5));
    int rises = 0;
    for(std::size_t i = 1; i < v.size(); i++)
      rises += *v[i] > *v[i - 1];
    CHECK(rises == 1); // falling all along, one jump back up
  }

  SECTION("retrigger: back to the start of the cycle")
  {
    auto lfo = make(Sin);
    int64_t t = 0;
    run(lfo, 37, t);
    lfo.inputs.retrigger.value.emplace();
    lfo.outputs.out.value.reset();
    lfo(make_flicks_tick(t, t + dt, 64));
    lfo.inputs.retrigger.value.reset();
    CHECK(*lfo.outputs.out.value == Approx(0.).margin(1e-6)); // sin(0)
  }

  SECTION("drift: smooth, bounded, and not a repeating cycle")
  {
    auto lfo = make(Drift, 0.3f);
    int64_t t = 0;
    const auto v = run(lfo, 400, t);
    float max_step = 0.f;
    for(std::size_t i = 1; i < v.size(); i++)
    {
      REQUIRE(v[i].has_value());
      CHECK(*v[i] >= -1.f);
      CHECK(*v[i] <= 1.f);
      max_step = std::max(max_step, std::abs(*v[i] - *v[i - 1]));
    }
    CHECK(max_step < 0.2f); // no jumps at 10 ms
    // Cycle 1 and cycle 2 differ: it wanders
    float diff = 0.f;
    for(int i = 0; i < 100; i++)
      diff += std::abs(*v[100 + i] - *v[200 + i]);
    CHECK(diff > 1.f);
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

TEST_CASE("MathAudioFilter: more channels than the initial two", "[fx][audio][exprtk]")
{
  // The ExprTK vector views for x / out / px are created against the node's
  // initial 2-channel buffers. Growing the bus has to grow the views too:
  // otherwise the expression only ever sees the first two channels.
  static constexpr int N = 4;
  static constexpr int C = 6;
  std::array<std::array<double, N>, C> in{}, out{};
  double* ins[C]{};
  double* outs[C]{};
  for(int c = 0; c < C; c++)
  {
    in[c].fill(c + 1);
    ins[c] = in[c].data();
    outs[c] = out[c].data();
  }

  filter_harness h;
  h.wire(ins, outs, C);
  h.node.inputs.expr.value
      = "var n := x[];\nfor(var i := 0; i < n; i += 1) { out[i] := x[i] * 10; }";
  h.run(N);

  for(int c = 0; c < C; c++)
    for(int i = 0; i < N; i++)
      CHECK(out[c][i] == Approx((c + 1) * 10.));
}

TEST_CASE("MathAudioFilter: x[] reports the actual channel count", "[fx][audio][exprtk]")
{
  static constexpr int N = 2;
  static constexpr int C = 4;
  std::array<std::array<double, N>, C> in{}, out{};
  double* ins[C]{};
  double* outs[C]{};
  for(int c = 0; c < C; c++)
  {
    ins[c] = in[c].data();
    outs[c] = out[c].data();
  }

  filter_harness h;
  h.wire(ins, outs, C);
  h.node.inputs.expr.value = "for(var i := 0; i < x[]; i += 1) { out[i] := x[]; }";
  h.run(N);

  for(int c = 0; c < C; c++)
    CHECK(out[c][0] == Approx((double)C));
}

TEST_CASE("MathAudioFilter: a stereo expression on a mono bus is not fatal", "[fx][audio][exprtk]")
{
  // The shipped "Crude Lowpass" / "Tan Disto" presets index x[1] / out[1]
  // explicitly: on a mono bus they cannot be compiled, which must leave the
  // output untouched rather than reading past the one-element vector view.
  static constexpr int N = 4;
  std::array<double, N> in{1, 2, 3, 4};
  std::array<double, N> out{-1, -1, -1, -1};
  double* ins[1]{in.data()};
  double* outs[1]{out.data()};

  filter_harness h;
  h.wire(ins, outs, 1);
  h.node.inputs.expr.value
      = "out[0] := clamp(-1,  tan(x[0]*a), 1);\nout[1] := clamp(-1,  tan(x[1]*a), 1);";
  h.run(N);

  for(int i = 0; i < N; i++)
    CHECK(out[i] == -1.);
}

TEST_CASE("MathAudioFilter: shipped presets run on a stereo bus", "[fx][audio][exprtk][presets]")
{
  struct preset
  {
    const char* expr;
    // "Aggressive shaping" raises the (bipolar) input to a fractional power,
    // which is NaN for negative samples: that is the preset's own doing, so
    // only its ability to run is checked.
    bool finite;
  };
  const preset presets[]{
      {"out := sin ((c +100 a) (x ^ (b x)))", false},
      {"out := 2 (100 a x) ^ 2 - 1", true},
      {"out := 4 (100 a x) ^ 3 - 3 (100 a x) ", true},
      {"out := 8 (100 a x) ^ 4 - 8 (100 a x) ^ 2 + 1", true},
      {"out[0] := clamp(-1,  ((1-2*a) * x[0]+2*a*px[0]), 1);\n"
       "out[1] := clamp(-1,  ((1-2*a) * x[1]+2*a*px[1]), 1);",
       true},
      {"out[0] := clamp(-1,  (x[0]*x[0]*x[0]*a), 1);\n"
       "out[1] := clamp(-1,  (x[1]*x[1]*x[1]*a), 1);",
       true},
      {"out := (round(x 50 a) / (50 a))", true},
      {"out := erf(x *(1 + 100 a)) / (1 + 100 a)", true},
      {"\nout[0] := clamp(-1,  tan(x[0]*log(1 + 200 * a)), 1);\n"
       "out[1] := clamp(-1,  tan(x[1]*log(1 + 200 * a)), 1);",
       true},
      {"out := (100 a x) / (1 + abs(100 a x))", true},
      {"out := 1.5 (100 a x) - 0.5 ((100 a x) ^ 3)", true},
      {"out[0] := clamp(-1,  sin(x[0]*a), 1);\nout[1] := clamp(-1,  sin(x[1]*a), 1);",
       true},
      {"\nout[0] := clamp(-1,  tan(x[0]*a), 1);\nout[1] := clamp(-1,  tan(x[1]*a), 1);",
       true},
  };

  static constexpr int N = 16;
  for(const auto& [expr, finite] : presets)
  {
    INFO(expr);
    std::array<double, N> l{}, r{}, ol{}, or_{};
    for(int i = 0; i < N; i++)
      l[i] = r[i] = std::sin(i * 0.3);
    double* ins[2]{l.data(), r.data()};
    double* outs[2]{ol.data(), or_.data()};

    filter_harness h;
    h.wire(ins, outs, 2);
    h.node.inputs.expr.value = expr;
    h.node.inputs.a.value = 0.5f;
    h.node.inputs.b.value = 0.5f;
    h.node.inputs.c.value = 0.5f;
    h.run(N);

    if(finite)
      for(int i = 0; i < N; i++)
        CHECK_FALSE(std::isnan(ol[i]));
  }
}
