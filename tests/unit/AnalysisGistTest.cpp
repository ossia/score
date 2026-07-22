#include <Analysis/Envelope.hpp>
#include <Analysis/GistState.hpp>
#include <Analysis/Helpers.hpp>

#include <Gist.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <complex>
#include <limits>
#include <vector>

using Catch::Approx;

namespace
{
constexpr double INV_SQRT2 = 0.70710678118654752440;

struct MultiBuf
{
  std::vector<std::vector<double>> ch;
  std::vector<double*> ptrs;

  Analysis::audio_in bus()
  {
    ptrs.clear();
    for(auto& c : ch)
      ptrs.push_back(c.data());
    Analysis::audio_in b;
    b.samples = ptrs.data();
    b.channels = (int)ptrs.size();
    return b;
  }
};

std::vector<double> make_sine(int n, double amp, double cycles, double shift = 0.5)
{
  std::vector<double> v(n);
  for(int i = 0; i < n; i++)
    v[i] = amp * std::sin(2.0 * M_PI * cycles * (i + shift) / n);
  return v;
}

std::vector<double> gist_hann(int n)
{
  std::vector<double> w(n);
  for(int i = 0; i < n; i++)
    w[i] = 0.5 * (1.0 - std::cos(2.0 * M_PI * (double(i) / double(n - 1))));
  return w;
}

// Reference DFT magnitude of the windowed frame at one bin.
double ref_dft_mag(const std::vector<double>& x, const std::vector<double>& w, int bin)
{
  const int n = (int)x.size();
  std::complex<double> acc{};
  for(int i = 0; i < n; i++)
  {
    const double ph = -2.0 * M_PI * double(bin) * double(i) / double(n);
    acc += x[i] * w[i] * std::complex<double>{std::cos(ph), std::sin(ph)};
  }
  return std::abs(acc);
}

struct PulseCounter
{
  int count = 0;
  void operator()() { ++count; }
};

float mono(const Analysis::value_out& out)
{
  auto* f = ossia::get_if<float>(&out.value);
  REQUIRE(f);
  return *f;
}

std::array<float, 2> stereo(const Analysis::value_out& out)
{
  auto* a = ossia::get_if<std::array<float, 2>>(&out.value);
  REQUIRE(a);
  return *a;
}
}

TEST_CASE(
    "GistState: RMS, peak and zero-crossings of a known sine (mono)",
    "[analysis][gist][envelope]")
{
  constexpr int N = 512, FS = 48000;
  constexpr double A = 0.5, K = 8; // 8 cycles per frame = 750 Hz

  MultiBuf buf;
  buf.ch.push_back(make_sine(N, A, K));
  auto bus = buf.bus();

  Analysis::GistState st{N, FS};
  Analysis::value_out out;

  // RMS over an integer number of periods is exactly A/sqrt(2).
  st.process<&Gist<double>::rootMeanSquare>(bus, 1.f, 0.f, out, N);
  CHECK(mono(out) == Approx(A * INV_SQRT2).margin(1e-6));

  // Peak energy is the max absolute sample of the frame.
  double expected_peak = 0;
  for(double s : buf.ch[0])
    expected_peak = std::max(expected_peak, std::abs(s));
  st.process<&Gist<double>::peakEnergy>(bus, 1.f, 0.f, out, N);
  CHECK(mono(out) == Approx(expected_peak).margin(1e-6));

  st.process<&Gist<double>::zeroCrossingRate>(bus, 1.f, 0.f, out, N);
  CHECK(mono(out) == Approx(15.0).margin(1e-9));
}

TEST_CASE(
    "GistState: gain scales and gate mutes the analyzed signal",
    "[analysis][gist][envelope]")
{
  constexpr int N = 512, FS = 48000;
  constexpr double A = 0.5;

  MultiBuf buf;
  buf.ch.push_back(make_sine(N, A, 8));
  auto bus = buf.bus();

  Analysis::GistState st{N, FS};
  Analysis::value_out out;

  // gain 2 doubles the RMS
  st.process<&Gist<double>::rootMeanSquare>(bus, 2.f, 0.f, out, N);
  CHECK(mono(out) == Approx(2.0 * A * INV_SQRT2).margin(1e-6));

  // A gate above the (post-gain) peak zeroes every sample.
  st.process<&Gist<double>::rootMeanSquare>(bus, 1.f, 0.6f, out, N);
  CHECK(mono(out) == Approx(0.0).margin(1e-12));
  st.process<&Gist<double>::peakEnergy>(bus, 1.f, 0.6f, out, N);
  CHECK(mono(out) == Approx(0.0).margin(1e-12));
}

TEST_CASE(
    "Gist FFT: sine magnitude peaks in its bin, DC in bin 0, values match a "
    "reference DFT",
    "[analysis][gist][fft]")
{
  constexpr int N = 512, FS = 48000;
  constexpr double A = 1.0;
  constexpr int K = 32; // bin-centered tone: 3 kHz at 48 kHz / 512

  const auto w = gist_hann(N);

  SECTION("bin-centered sine")
  {
    const auto x = make_sine(N, A, K, 0.0);

    Gist<double> g{N, FS};
    g.processAudioFrame(x.data(), N);
    const auto& mag = g.getMagnitudeSpectrum();
    REQUIRE((int)mag.size() == N / 2);

    // argmax lands exactly on bin K
    int argmax = 0;
    for(int i = 1; i < N / 2; i++)
      if(mag[i] > mag[argmax])
        argmax = i;
    CHECK(argmax == K);

    // Hann window: adjacent bins carry ~half the peak
    CHECK(mag[K - 1] / mag[K] == Approx(0.5).margin(0.05));
    CHECK(mag[K + 1] / mag[K] == Approx(0.5).margin(0.05));

    // Away from the tone the spectrum is (near) empty
    for(int b : {0, 8, 16, 64, 128, 255})
      CHECK(mag[b] < 0.02 * mag[K]);

    // Exact values against the reference DFT of the windowed frame
    for(int b : {0, 16, 31, 32, 33, 100, 255})
      CHECK(mag[b] == Approx(ref_dft_mag(x, w, b)).margin(1e-6 * N).epsilon(1e-7));
  }

  SECTION("DC goes to bin 0")
  {
    std::vector<double> x(N, 0.7);

    Gist<double> g{N, FS};
    g.processAudioFrame(x.data(), N);
    const auto& mag = g.getMagnitudeSpectrum();

    int argmax = 0;
    for(int i = 1; i < N / 2; i++)
      if(mag[i] > mag[argmax])
        argmax = i;
    CHECK(argmax == 0);

    // |X[0]| of a windowed constant = level * sum(window)
    double wsum = 0;
    for(double v : w)
      wsum += v;
    CHECK(mag[0] == Approx(0.7 * wsum).epsilon(1e-9));
  }
}

TEST_CASE(
    "GistState: spectral centroid of a pure tone lands on its bin index",
    "[analysis][gist][spectral]")
{
  constexpr int N = 512, FS = 48000;
  constexpr int K = 32;

  Analysis::GistState st{N, FS};
  Analysis::value_out out;

  MultiBuf buf;
  buf.ch.push_back(make_sine(N, 1.0, K, 0.0));
  auto bus = buf.bus();

  // NB: Gist's spectralCentroid is expressed in *bins*, not Hz.
  st.process<&Gist<double>::spectralCentroid>(bus, 1.f, 0.f, out, N);
  CHECK(mono(out) == Approx((double)K).margin(1.0));

  // DC: all the energy sits at the bottom of the spectrum.
  MultiBuf dc;
  dc.ch.push_back(std::vector<double>(N, 1.0));
  auto dcbus = dc.bus();
  st.process<&Gist<double>::spectralCentroid>(dcbus, 1.f, 0.f, out, N);
  CHECK(mono(out) < 1.0);
}

TEST_CASE(
    "GistState: spectral flatness and crest separate tones from flat spectra",
    "[analysis][gist][spectral]")
{
  constexpr int N = 512, FS = 48000;

  Analysis::GistState st{N, FS};
  Analysis::value_out out;

  MultiBuf delta;
  delta.ch.push_back(std::vector<double>(N, 0.0));
  delta.ch[0][N / 2] = 1.0;
  auto dbus = delta.bus();

  st.process<&Gist<double>::spectralFlatness>(dbus, 1.f, 0.f, out, N);
  CHECK(mono(out) == Approx(1.0).margin(1e-6));
  st.process<&Gist<double>::spectralCrest>(dbus, 1.f, 0.f, out, N);
  CHECK(mono(out) == Approx(1.0).margin(1e-6));

  // A pure tone concentrates its energy: low flatness, high crest.
  MultiBuf tone;
  tone.ch.push_back(make_sine(N, 1.0, 32, 0.0));
  auto tbus = tone.bus();

  st.process<&Gist<double>::spectralFlatness>(tbus, 1.f, 0.f, out, N);
  CHECK(mono(out) < 0.7);
  st.process<&Gist<double>::spectralCrest>(tbus, 1.f, 0.f, out, N);
  CHECK(mono(out) > 20.0);
}

TEST_CASE("GistState: Yin pitch tracking of a 440 Hz sine", "[analysis][gist][pitch]")
{
  constexpr int N = 2048, FS = 48000;
  constexpr double F = 440.0;

  Analysis::GistState st{N, FS};
  Analysis::value_out out;

  MultiBuf buf;
  buf.ch.push_back(make_sine(N, 0.8, F * N / FS));
  auto bus = buf.bus();

  // Same call the Pitch node makes (the no-gain process overload).
  st.process<&Gist<double>::pitch>(bus, out, N);
  CHECK(mono(out) == Approx(F).margin(5.0));
}

TEST_CASE(
    "GistState: stereo gain/gate analysis reports per-channel values",
    "[analysis][gist][stereo]")
{
  constexpr int N = 512, FS = 48000;

  MultiBuf buf;
  buf.ch.push_back(make_sine(N, 0.4, 8));
  buf.ch.push_back(make_sine(N, 0.8, 8));
  auto bus = buf.bus();

  Analysis::GistState st{N, FS};
  Analysis::value_out out;
  st.process<&Gist<double>::rootMeanSquare>(bus, 1.f, 0.f, out, N);

  auto v = stereo(out);
  CHECK(v[0] == Approx(0.4 * INV_SQRT2).margin(1e-6));
  CHECK(v[1] == Approx(0.8 * INV_SQRT2).margin(1e-6));
}

TEST_CASE(
    "GistState: stereo no-gain analysis analyzes the right channel",
    "[analysis][gist][stereo]")
{
  constexpr int N = 512, FS = 48000;

  MultiBuf buf;
  buf.ch.push_back(make_sine(N, 0.4, 8));
  buf.ch.push_back(make_sine(N, 0.8, 8));
  auto bus = buf.bus();

  Analysis::GistState st{N, FS};
  Analysis::value_out out;
  st.process<&Gist<double>::rootMeanSquare>(bus, out, N);

  auto v = stereo(out);
  CHECK(v[0] == Approx(0.4 * INV_SQRT2).margin(1e-6));
  CHECK(v[1] == Approx(0.8 * INV_SQRT2).margin(1e-6));
}

TEST_CASE(
    "GistState: stereo pulse analysis analyzes the right channel",
    "[analysis][gist][stereo]")
{
  constexpr int N = 512, FS = 48000;

  MultiBuf buf;
  buf.ch.push_back(make_sine(N, 0.4, 8));
  buf.ch.push_back(make_sine(N, 0.8, 8));
  auto bus = buf.bus();

  Analysis::GistState st{N, FS};
  Analysis::value_out out;
  PulseCounter pulse;
  st.process<&Gist<double>::rootMeanSquare>(bus, 1.f, 0.f, out, pulse, N);

  auto v = stereo(out);
  CHECK(v[0] == Approx(0.4 * INV_SQRT2).margin(1e-6));
  CHECK(v[1] == Approx(0.8 * INV_SQRT2).margin(1e-6));
}

TEST_CASE(
    "GistState: pulse fires when the tracked value reaches 1",
    "[analysis][gist][onset]")
{
  constexpr int N = 512, FS = 48000;

  Analysis::GistState st{N, FS};
  Analysis::value_out out;

  // peak = 2.0 >= 1 -> bang
  MultiBuf loud;
  loud.ch.push_back(make_sine(N, 2.0, 8));
  auto lbus = loud.bus();
  PulseCounter p1;
  st.process<&Gist<double>::peakEnergy>(lbus, 1.f, 0.f, out, p1, N);
  CHECK(p1.count == 1);

  // peak = 0.5 < 1 -> no bang
  MultiBuf quiet;
  quiet.ch.push_back(make_sine(N, 0.5, 8));
  auto qbus = quiet.bus();
  PulseCounter p2;
  st.process<&Gist<double>::peakEnergy>(qbus, 1.f, 0.f, out, p2, N);
  CHECK(p2.count == 0);
}

TEST_CASE(
    "GistState: 4-channel analysis returns one value per channel",
    "[analysis][gist][multichannel]")
{
  constexpr int N = 512, FS = 48000;
  const double amps[4] = {0.1, 0.2, 0.3, 0.4};

  MultiBuf buf;
  for(double a : amps)
    buf.ch.push_back(make_sine(N, a, 8));
  auto bus = buf.bus();

  Analysis::GistState st{N, FS};
  Analysis::value_out out;
  st.process<&Gist<double>::rootMeanSquare>(bus, 1.f, 0.f, out, N);

  auto* v = ossia::get_if<Analysis::analysis_vector>(&out.value);
  REQUIRE(v);
  REQUIRE(v->size() == 4);
  for(int c = 0; c < 4; c++)
    CHECK((*v)[c] == Approx(amps[c] * INV_SQRT2).margin(1e-6));
}

TEST_CASE(
    "EnvelopeFollower: exact half-life step response",
    "[analysis][envelope-follower]")
{
  Analysis::EnvelopeFollower env;
  halp::setup s{};
  s.rate = 48000;
  env.prepare(s);

  Analysis::EnvelopeFollower::inputs ins{}; // defaults: 50 ms up, 15 ms down
  REQUIRE(ins.a.value == Approx(50.0));
  REQUIRE(ins.b.value == Approx(15.0));

  double y = 0;
  for(int i = 0; i < 2400; i++)
    y = env(1.0, ins, {});
  CHECK(y == Approx(0.5).margin(1e-6));

  // ... and 0.75 after another 50 ms.
  for(int i = 0; i < 2400; i++)
    y = env(1.0, ins, {});
  CHECK(y == Approx(0.75).margin(1e-6));

  // Release: 15 ms half-life. After 720 samples of silence y halves.
  for(int i = 0; i < 720; i++)
    y = env(0.0, ins, {});
  CHECK(y == Approx(0.375).margin(1e-6));

  // rate <= 0 resets to zero instead of dividing by zero.
  Analysis::EnvelopeFollower dead;
  halp::setup zs{};
  zs.rate = 0;
  dead.prepare(zs);
  CHECK(dead(1.0, ins, {}) == 0.0);
}

TEST_CASE(
    "GistState: hostile buffers (NaN, denormals, empty, resize) do not crash",
    "[analysis][gist][fuzz]")
{
  constexpr int N = 512, FS = 48000;
  Analysis::GistState st{N, FS};
  Analysis::value_out out;

  // NaN-filled frame
  MultiBuf nan_buf;
  nan_buf.ch.push_back(
      std::vector<double>(N, std::numeric_limits<double>::quiet_NaN()));
  auto nbus = nan_buf.bus();
  st.process<&Gist<double>::rootMeanSquare>(nbus, 1.f, 0.f, out, N);
  st.process<&Gist<double>::spectralCentroid>(nbus, 1.f, 0.f, out, N);

  // Denormal-filled frame
  MultiBuf den;
  den.ch.push_back(std::vector<double>(N, 1e-320));
  auto dbus = den.bus();
  st.process<&Gist<double>::rootMeanSquare>(dbus, 1.f, 0.f, out, N);
  st.process<&Gist<double>::spectralFlatness>(dbus, 1.f, 0.f, out, N);

  // Zero channels
  MultiBuf empty;
  auto ebus = empty.bus();
  st.process<&Gist<double>::rootMeanSquare>(ebus, 1.f, 0.f, out, N);

  // Frame-size change re-inits the Gist FFT on the fly; values stay correct.
  MultiBuf small;
  small.ch.push_back(make_sine(256, 0.5, 4));
  auto sbus = small.bus();
  st.process<&Gist<double>::rootMeanSquare>(sbus, 1.f, 0.f, out, 256);
  CHECK(mono(out) == Approx(0.5 * INV_SQRT2).margin(1e-6));

  SUCCEED("no crash on hostile input");
}
