// P3R3 — DSP-correctness unit tests for the bundled avnd (avendish/halp) audio
// effects that ship with score through score-plugin-avnd (avnd_make_score in
// src/plugins/score-plugin-avnd/CMakeLists.txt).
//
// The avnd nodes are plain C++ structs: inputs/outputs port structs + a
// process operator(). They are constructed and run here directly, without the
// score wrapper, engine or any Qt application — buffers in, buffers out,
// asserting output samples against the closed-form math of each effect.
//
// Nodes covered (all in 3rdparty/avendish/examples/Advanced/Audio):
//  * ao::Gain         — per-sample out = in * gain            (exact)
//  * ao::AudioSum     — out[i] = sum over channels of in[c][i] (exact)
//  * ao::MonoMix      — out[i] = sum over ports/channels in*g  (exact)
//  * ao::StereoMixer  — linear pan law lp = g*(1-p), rp = g*p  (exact)
//  * ao::Silence      — channel count request plumbing
//
// Linear operations get exact (==) or 1-ulp-ish assertions; nothing here is
// allowed a loose tolerance since the math is pure multiply-accumulate.

#include <Advanced/Audio/AudioSum.hpp>
#include <Advanced/Audio/Gain.hpp>
#include <Advanced/Audio/MonoMix.hpp>
#include <Advanced/Audio/Silence.hpp>
#include <Advanced/Audio/StereoMixer.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <limits>
#include <vector>

using Catch::Approx;

TEST_CASE("ao::Gain scales every sample by exactly the gain value", "[avnd][audio][gain]")
{
  ao::Gain fx;
  ao::Gain::inputs in;

  SECTION("unity gain is a bit-exact passthrough")
  {
    in.gain.value = 1.f;
    for(double s : {0., 1., -1., 0.5, 1e-30, -1e-30})
      CHECK(fx(s, in) == s);
  }

  SECTION("gain k multiplies exactly")
  {
    in.gain.value = 0.5f;
    CHECK(fx(1.0, in) == 0.5);
    CHECK(fx(-2.0, in) == -1.0);
    CHECK(fx(0.25, in) == 0.125);

    in.gain.value = 2.5f;
    CHECK(fx(1.0, in) == 2.5);
    CHECK(fx(-0.5, in) == -1.25);
  }

  SECTION("zero gain fully mutes, including denormal input")
  {
    in.gain.value = 0.f;
    CHECK(fx(1.0, in) == 0.0);
    CHECK(fx(std::numeric_limits<double>::denorm_min(), in) == 0.0);
  }

  SECTION("an impulse through gain k has impulse response {k}")
  {
    in.gain.value = 0.75f;
    // impulse
    CHECK(fx(1.0, in) == 0.75);
    // and the tail stays exactly zero: the node is memoryless
    CHECK(fx(0.0, in) == 0.0);
    CHECK(fx(0.0, in) == 0.0);
  }

  SECTION("NaN input does not crash and propagates NaN")
  {
    in.gain.value = 1.f;
    CHECK(std::isnan(fx(std::numeric_limits<double>::quiet_NaN(), in)));
  }
}

TEST_CASE("ao::AudioSum sums all input channels into its mono output", "[avnd][audio][sum]")
{
  ao::AudioSum fx;

  static constexpr int N = 8;
  std::array<double, N> c0{1, 2, 3, 4, -1, -2, -3, -4};
  std::array<double, N> c1{0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5};
  std::array<double, N> c2{-1, 0, 1, 0, -1, 0, 1, 0};
  std::array<double*, 3> ins{c0.data(), c1.data(), c2.data()};

  std::array<double, N> out;
  out.fill(123.); // poison: the node must overwrite, not accumulate
  std::array<double*, 1> outs{out.data()};
  fx.outputs.audio.samples = outs.data();

  SECTION("three channels: out = c0 + c1 + c2, exact")
  {
    fx.inputs.audio.samples = ins.data();
    fx.inputs.audio.channels = 3;
    fx(N);
    for(int i = 0; i < N; i++)
      CHECK(out[i] == c0[i] + c1[i] + c2[i]);
  }

  SECTION("one channel: passthrough")
  {
    fx.inputs.audio.samples = ins.data();
    fx.inputs.audio.channels = 1;
    fx(N);
    for(int i = 0; i < N; i++)
      CHECK(out[i] == c0[i]);
  }

  SECTION("zero input channels: output is cleared to exactly 0")
  {
    fx.inputs.audio.samples = nullptr;
    fx.inputs.audio.channels = 0;
    fx(N);
    for(int i = 0; i < N; i++)
      CHECK(out[i] == 0.0);
  }

  SECTION("zero-length buffer does not crash and writes nothing")
  {
    fx.inputs.audio.samples = ins.data();
    fx.inputs.audio.channels = 3;
    fx(0);
    CHECK(out[0] == 123.); // untouched
  }

  SECTION("single-sample buffer")
  {
    fx.inputs.audio.samples = ins.data();
    fx.inputs.audio.channels = 2;
    fx(1);
    CHECK(out[0] == c0[0] + c1[0]);
    CHECK(out[1] == 123.); // rest untouched
  }
}

TEST_CASE("ao::MonoMix computes the exact gain-weighted sum of all bus channels", "[avnd][audio][mix]")
{
  ao::MonoMix fx;

  static constexpr int N = 4;
  // Bus 0: stereo
  std::array<double, N> b0l{1, 2, 3, 4};
  std::array<double, N> b0r{10, 20, 30, 40};
  std::array<double*, 2> b0{b0l.data(), b0r.data()};
  // Bus 1: mono
  std::array<double, N> b1m{-4, -3, -2, -1};
  std::array<double*, 1> b1{b1m.data()};

  fx.inputs.c0.samples = b0.data();
  fx.inputs.c0.channels = 2;
  fx.inputs.c1.samples = b1.data();
  fx.inputs.c1.channels = 1;
  // c2..c7 stay {nullptr, 0}

  std::array<double, N> out;
  out.fill(-99.);
  std::array<double*, 1> outs{out.data()};
  fx.outputs.audio.samples = outs.data();

  SECTION("per-bus gains weight every channel of that bus")
  {
    fx.inputs.g0.value = 0.5f;
    fx.inputs.g1.value = 0.25f;
    fx(N);
    for(int i = 0; i < N; i++)
    {
      const double expected
          = (b0l[i] + b0r[i]) * double(0.5f) + b1m[i] * double(0.25f);
      CHECK(out[i] == Approx(expected).margin(0));
    }
  }

  SECTION("all gains zero: exact silence")
  {
    fx.inputs.g0.value = 0.f;
    fx.inputs.g1.value = 0.f;
    fx(N);
    for(int i = 0; i < N; i++)
      CHECK(out[i] == 0.0);
  }

  SECTION("no connected bus at all: output cleared")
  {
    ao::MonoMix empty;
    empty.outputs.audio.samples = outs.data();
    out.fill(7.);
    empty(N);
    for(int i = 0; i < N; i++)
      CHECK(out[i] == 0.0);
  }

  SECTION("zero frames: no crash, nothing written")
  {
    fx.inputs.g0.value = 1.f;
    fx(0);
    CHECK(out[0] == -99.);
  }

  SECTION("NaN input does not crash; NaN propagates to the mix")
  {
    b1m[2] = std::numeric_limits<double>::quiet_NaN();
    fx.inputs.g0.value = 1.f;
    fx.inputs.g1.value = 1.f;
    fx(N);
    CHECK(std::isnan(out[2]));
    CHECK(out[0] == b0l[0] + b0r[0] + b1m[0]);
  }
}

TEST_CASE("ao::StereoMixer applies the linear pan law lp=g*(1-p), rp=g*p", "[avnd][audio][pan]")
{
  ao::StereoMixer fx;

  static constexpr int N = 4;
  std::array<double, N> mono{1., -0.5, 0.25, 2.};
  std::array<double*, 1> in_mono{mono.data()};

  std::array<double, N> outl, outr;
  std::array<double*, 2> outs{outl.data(), outr.data()};
  fx.outputs.audio.samples = outs.data();

  // Only bus 0 connected by default.
  fx.inputs.a0.samples = in_mono.data();
  fx.inputs.a0.channels = 1;
  // gains/pans for unconnected buses are irrelevant (channels == 0 -> skip)

  const auto run = [&](float gain, float pan) {
    fx.inputs.c0g.value = gain;
    fx.inputs.c0p.value = pan;
    fx(N);
  };

  SECTION("hard left (pan = 0): all signal to L, R exactly silent")
  {
    run(1.f, 0.f);
    for(int i = 0; i < N; i++)
    {
      CHECK(outl[i] == mono[i]);
      CHECK(outr[i] == 0.0);
    }
  }

  SECTION("hard right (pan = 1): all signal to R, L exactly silent")
  {
    run(1.f, 1.f);
    for(int i = 0; i < N; i++)
    {
      CHECK(outl[i] == 0.0);
      CHECK(outr[i] == mono[i]);
    }
  }

  SECTION("center (pan = 0.5): both channels at exactly half amplitude")
  {
    run(1.f, 0.5f);
    for(int i = 0; i < N; i++)
    {
      CHECK(outl[i] == mono[i] * (1. - double(0.5f)));
      CHECK(outr[i] == mono[i] * double(0.5f));
    }
  }

  SECTION("gain scales both sides of the pan law")
  {
    run(0.5f, 0.25f);
    for(int i = 0; i < N; i++)
    {
      const double lp = double(0.5f) * (1. - double(0.25f));
      const double rp = double(0.5f) * double(0.25f);
      CHECK(outl[i] == Approx(mono[i] * lp).margin(0));
      CHECK(outr[i] == Approx(mono[i] * rp).margin(0));
    }
  }

  SECTION("stereo input: L->left path, R->right path, separately panned")
  {
    std::array<double, N> l{1, 2, 3, 4}, r{8, 6, 4, 2};
    std::array<double*, 2> in_st{l.data(), r.data()};
    fx.inputs.a0.samples = in_st.data();
    fx.inputs.a0.channels = 2;
    run(1.f, 0.25f);
    for(int i = 0; i < N; i++)
    {
      CHECK(outl[i] == l[i] * (1. - double(0.25f)));
      CHECK(outr[i] == r[i] * double(0.25f));
    }
  }

  SECTION(">2 channels are downmixed (summed) then panned")
  {
    std::array<double, N> c0{1, 1, 1, 1}, c1{2, 2, 2, 2}, c2{3, 3, 3, 3};
    std::array<double*, 3> in3{c0.data(), c1.data(), c2.data()};
    fx.inputs.a0.samples = in3.data();
    fx.inputs.a0.channels = 3;
    run(1.f, 0.5f);
    for(int i = 0; i < N; i++)
    {
      CHECK(outl[i] == 6. * (1. - double(0.5f)));
      CHECK(outr[i] == 6. * double(0.5f));
    }
  }

  SECTION("two buses mix additively into the same stereo pair")
  {
    std::array<double, N> m2{0.5, 0.5, 0.5, 0.5};
    std::array<double*, 1> in2{m2.data()};
    fx.inputs.a1.samples = in2.data();
    fx.inputs.a1.channels = 1;
    fx.inputs.c1g.value = 1.f;
    fx.inputs.c1p.value = 1.f; // hard right
    run(1.f, 0.f);             // bus 0 hard left
    for(int i = 0; i < N; i++)
    {
      CHECK(outl[i] == mono[i]);
      CHECK(outr[i] == m2[i]);
    }
  }

  SECTION("zero frames: no crash")
  {
    run(1.f, 0.5f);
    fx(0);
    SUCCEED("no crash on empty buffer");
  }
}

TEST_CASE("ao::Silence requests exactly the configured channel count", "[avnd][audio][silence]")
{
  ao::Silence fx;
  int requested = -1;
  fx.outputs.audio.request_channels = [&](int n) {
    requested = n;
    fx.outputs.audio.channels = n; // emulate the host honoring the request
  };

  fx.inputs.channels.value = 5;
  fx.prepare(halp::setup{.input_channels = 0, .output_channels = 2, .frames = 64, .rate = 48000.});
  CHECK(requested == 5);

  // Once the host honored the request, the node settles.
  requested = -1;
  fx();
  CHECK(requested == -1);

  // Changing the control re-requests on the next tick.
  fx.inputs.channels.value = 3;
  fx();
  CHECK(requested == 3);
}
