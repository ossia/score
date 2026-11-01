// Audio meter, spectrum, loudness and scope: each ballistic against the
// calibration points of its standard, and the nodes' processing without
// allocation.

#include "AllocCounter.hpp"

#include <Ui/VUMeter.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <numbers>
#include <string_view>
#include <vector>

using namespace ossia::meters;
using Catch::Approx;

namespace
{
constexpr double rate = 48000.;

double to_db(double lin)
{
  return ossia::detail::LinearGainToDecibels(std::max(lin, 1e-10));
}
double from_db(double db)
{
  return ossia::detail::DecibelsToLinearGain(db);
}

std::vector<double> sine(double freq, double amp, double seconds, double phase = 0.)
{
  std::vector<double> v(std::size_t(seconds * rate));
  for(std::size_t i = 0; i < v.size(); i++)
    v[i] = amp * std::sin(2. * std::numbers::pi * freq * i / rate + phase);
  return v;
}
}

TEST_CASE("VU: 99% of a tone after 300 ms, overshoot within IEC 60268-17", "[meters]")
{
  vu m;
  m.prepare(rate);
  const auto s = sine(1000., 1., 1.);
  double highest = 0, at_300ms = 0;
  for(std::size_t i = 0; i < s.size(); i++)
  {
    m.process(&s[i], 1);
    highest = std::max(highest, m.level());
    if(i == std::size_t(0.3 * rate))
      at_300ms = m.level();
  }
  const double rms = 1. / std::numbers::sqrt2;
  CHECK(at_300ms / rms == Approx(0.99).margin(0.005));
  CHECK(highest / rms > 1.01);
  CHECK(highest / rms < 1.015);
  CHECK(m.level() / rms == Approx(1.).margin(0.002));
}

TEST_CASE("PPM: tone bursts and return time per IEC 60268-10", "[meters]")
{
  for(auto t : {ppm::type_i, ppm::type_ii})
  {
    ppm p;
    p.prepare(rate, t);
    // A burst as long as the integration time reads 2 dB under the steady tone
    ppm burst = p;
    CHECK(
        to_db(burst.tone_reading(rate, ppm::integration_time(t)) * p.gain)
        == Approx(-2.).margin(0.05));
    ppm steady = p;
    CHECK(to_db(steady.tone_reading(rate, 0.5) * p.gain) == Approx(0.).margin(0.05));

    ppm fall = p;
    const auto tone = sine(5000., 1., 0.5);
    fall.process(tone.data(), int(tone.size()));
    const double start = to_db(fall.level());
    const std::vector<double> silence(
        std::size_t((t == ppm::type_i ? 1.5 : 2.8) * rate), 0.);
    fall.process(silence.data(), int(silence.size()));
    CHECK(to_db(fall.level()) - start == Approx(t == ppm::type_i ? -20. : -24.).margin(0.1));
  }
}

TEST_CASE("True peak: inter-sample peaks per BS.1770", "[meters]")
{
  true_peak tp;
  tp.prepare(rate);
  // A quarter-rate sine sampled at 45 degrees: every sample is at -3 dB
  const auto s = sine(rate / 4., 1., 0.1, std::numbers::pi / 4.);
  double sample_peak = 0;
  for(double x : s)
    sample_peak = std::max(sample_peak, std::abs(x));
  tp.process(s.data(), int(s.size()));
  CHECK(to_db(sample_peak) == Approx(-3.01).margin(0.01));
  CHECK(to_db(tp.block_peak) == Approx(0.).margin(0.2));

  for(double f : {997., 5000., 15000.})
  {
    true_peak t2;
    t2.prepare(rate);
    const auto x = sine(f, 0.5, 0.1, 0.3);
    t2.process(x.data(), int(x.size()));
    CHECK(to_db(t2.block_peak) == Approx(-6.02).margin(0.1));
  }
}

TEST_CASE("RMS and correlation", "[meters]")
{
  rms r;
  r.prepare(rate, 0.6, true);
  const auto s = sine(1000., 0.5, 3.);
  r.process(s.data(), int(s.size()));
  // AES-17: a sine reads its peak
  CHECK(to_db(r.level()) == Approx(-6.02).margin(0.02));

  correlation c;
  c.prepare(rate);
  std::vector<double> inv(s.size());
  for(std::size_t i = 0; i < s.size(); i++)
    inv[i] = -s[i];
  c.process(s.data(), inv.data(), int(s.size()));
  CHECK(c.value() == Approx(-1.).margin(1e-6));
  c.process(s.data(), s.data(), int(s.size()));
  CHECK(c.value() == Approx(1.).margin(1e-3));
}

TEST_CASE("True peak: the sample peak at 192 kHz, where it is not oversampled", "[meters]")
{
  true_peak tp;
  tp.prepare(192000.);
  CHECK(tp.factor == 1);
  std::vector<double> x(1000);
  for(std::size_t i = 0; i < x.size(); i++)
    x[i] = 0.7 * std::sin(2. * std::numbers::pi * 1000. * i / 192000. + 0.1);
  double sample_peak = 0;
  for(double v : x)
    sample_peak = std::max(sample_peak, std::abs(v));
  tp.process(x.data(), int(x.size()));
  CHECK(tp.block_peak == sample_peak);

  // Fed one sample at a time, the oversampled meter reads what a block does
  true_peak a, b;
  a.prepare(rate);
  b.prepare(rate);
  const auto s = sine(9000., 0.8, 0.05, 0.4);
  a.process(s.data(), int(s.size()));
  double peak = 0;
  for(double v : s)
  {
    b.process(&v, 1);
    peak = std::max(peak, b.block_peak);
  }
  CHECK(peak == Approx(a.block_peak).epsilon(1e-12));
}

TEST_CASE("PPM: preparing again for the same rate keeps the design", "[meters]")
{
  ppm p;
  p.prepare(rate, ppm::type_ii);
  const double attack = p.attack, gain = p.gain;
  const auto tone = sine(5000., 1., 0.1);
  p.process(tone.data(), int(tone.size()));
  CHECK(p.env > 0);
  p.prepare(rate, ppm::type_ii);
  CHECK(p.env == 0);
  CHECK(p.attack == attack);
  CHECK(p.gain == gain);
  p.prepare(rate, ppm::type_i);
  CHECK(p.attack != attack);
}

TEST_CASE("Audio meter: no allocation while processing, in any mode", "[meters][ui]")
{
  using namespace Ui::VUMeter;
  Node node;
  node.prepare({.input_channels = 2, .frames = 256, .rate = rate});
  auto l = sine(1000., 0.45, 256. / rate);
  auto r = sine(1000., 0.25, 256. / rate);
  double* chans[2]{l.data(), r.data()};
  node.inputs.audio.samples = chans;
  node.inputs.audio.channels = 2;
  node.monitor->viewers = 1;

  int level_lists = 0;
  for(int m = 0; m <= int(Mode::Bits); m++)
  {
    alloc_counter::scope counter;
    // A mode switch resets the followers, which may not allocate either
    node.inputs.mode.value = Mode(m);
    for(int i = 0; i < 400; i++)
    {
      // As the host does before each tick
      node.outputs.levels.value.reset();
      const int before = counter.allocations();
      node(256);
      if(node.outputs.levels.value)
        level_lists++;
      else
        CHECK(counter.allocations() == before);
    }
    // A hold or fall change only retunes the followers
    node.inputs.hold.value = m;
    node.inputs.fall.value = 10 + m;
    node.outputs.levels.value.reset();
    const int before = counter.allocations();
    node(256);
    if(!node.outputs.levels.value)
      CHECK(counter.allocations() == before);
  }
  // The level lists go out once per 1/30 s of signal: every seventh tick
  CHECK(level_lists == Approx(12. * 401. / 7.).margin(2.));

  Frame f;
  REQUIRE(node.monitor->frames.consume(f));
  CHECK(f.channels == 2);
  CHECK(f.mode == Mode::Bits);
  // 0.45 of full scale needs 22 bits: bit 21 is its highest
  CHECK((f.bits[0] >> 22) == 0);
  CHECK((f.bits[0] >> 21) == 1);
}

TEST_CASE("Audio meter: a fall rate change keeps the levels", "[meters][ui]")
{
  using namespace Ui::VUMeter;
  Node node;
  node.prepare({.input_channels = 1, .frames = 256, .rate = rate});
  auto x = sine(1000., 0.5, 256. / rate);
  double* chans[1]{x.data()};
  node.inputs.audio.samples = chans;
  node.inputs.audio.channels = 1;
  for(int i = 0; i < 100; i++)
    node(256);
  const double level = node.channels[0].peak.level;
  const double rms = node.channels[0].rms.ms;
  CHECK(level > 0.45);
  node.inputs.fall.value = 40.;
  node.inputs.hold.value = 3.;
  node(256);
  CHECK(node.channels[0].peak.level >= level * 0.99);
  CHECK(node.channels[0].rms.ms == Approx(rms).epsilon(0.01));
  CHECK(node.channels[0].hold.hold_frames == Approx(3. * rate));
}

TEST_CASE("Audio meter: scales put the alignment level where each standard does", "[meters][ui]")
{
  using namespace Ui::VUMeter;
  const double ref = -18.;
  const double align = from_db(ref);
  CHECK(scaleFor(Mode::VU, -60, 0).value(align, ref) == Approx(0.));
  CHECK(scaleFor(Mode::PpmDin, -60, 0).value(align, ref) == Approx(-9.));
  CHECK(scaleFor(Mode::PpmNordic, -60, 0).value(align, ref) == Approx(0.));
  CHECK(scaleFor(Mode::PpmEbu, -60, 0).value(align, ref) == Approx(0.));
  CHECK(scaleFor(Mode::PpmBbc, -60, 0).value(align, ref) == Approx(4.));
  CHECK(scaleFor(Mode::PpmBbc, -60, 0).value(from_db(ref + 8.), ref) == Approx(6.));
  // K-System: 0 at -k dBFS, whatever the reference
  CHECK(scaleFor(Mode::K20, -60, 0).value(from_db(-20.), ref) == Approx(0.).margin(1e-9));
  CHECK(scaleFor(Mode::K14, -60, 0).value(from_db(-14.), ref) == Approx(0.).margin(1e-9));
  CHECK(scaleFor(Mode::PeakRms, -60, 0).value(from_db(-6.), ref) == Approx(-6.));
}

#include <Ui/Monitor/SpectrumAnalysis.hpp>

TEST_CASE("Spectrum: a full-scale sine reads 0 dBFS in any window", "[meters][spectrum]")
{
  using namespace Ui::Monitor;
  for(auto w : {Window::Hann, Window::BlackmanHarris, Window::FlatTop})
  {
    SpectrumAnalysis a;
    a.prepare(4096, rate, w);
    // On a bin: exact for every window
    const double on_bin = a.binFrequency(100);
    auto x = sine(on_bin, 1., 4096. / rate);
    a.analyse(x.data());
    CHECK(10. * std::log10(a.power[100]) == Approx(0.).margin(0.01));

    // Between two bins: only the flat top keeps the level
    const auto y = sine(a.binFrequency(100.5), 1., 4096. / rate);
    a.analyse(y.data());
    const double highest = std::max(a.power[100], a.power[101]);
    if(w == Window::FlatTop)
      CHECK(10. * std::log10(highest) == Approx(0.).margin(0.05));
    else
      CHECK(10. * std::log10(highest) > -1.5);

    // The band holding the sine sums its energy back to the sine's level
    a.analyse(x.data());
    CHECK(10. * std::log10(a.bandPower(on_bin / 1.12, on_bin * 1.12)) == Approx(0.).margin(0.1));
  }
}

TEST_CASE("Spectrum: third-octave centres and the fundamental of a tone", "[meters][spectrum]")
{
  using namespace Ui::Monitor;
  CHECK(ThirdOctave::centre(17) == Approx(1000.));
  CHECK(ThirdOctave::centre(0) == Approx(19.95).margin(0.01));
  CHECK(ThirdOctave::centre(30) == Approx(19952.6).margin(0.1));

  SpectrumAnalysis a;
  a.prepare(8192, rate, Window::Hann);
  std::vector<double> x(8192);
  for(std::size_t i = 0; i < x.size(); i++)
  {
    const double t = 2. * std::numbers::pi * 442. * i / rate;
    x[i] = 0.5 * std::sin(t) + 0.3 * std::sin(2 * t) + 0.2 * std::sin(3 * t);
  }
  a.analyse(x.data());
  std::vector<float> frame(x.begin(), x.begin() + 2048);
  PitchTracker pitch;
  CHECK(pitch(frame, rate) == Approx(442.).margin(0.5));
  double cents{};
  CHECK(noteName(442., &cents) == "A4");
  CHECK(cents == Approx(7.85).margin(0.01));

  CHECK(std::string_view{ThirdOctave::labels[2]} == "31.5");
  CHECK(std::string_view{ThirdOctave::labels[18]} == "1.25k");
  CHECK(std::string_view{ThirdOctave::labels[22]} == "3.15k");
  CHECK(std::string_view{ThirdOctave::labels[28]} == "12.5k");
}

TEST_CASE("Spectrum: bands narrower than a bin read the same level per Hz", "[meters][spectrum]")
{
  using namespace Ui::Monitor;
  SpectrumAnalysis a;
  a.prepare(1024, rate, Window::Hann);
  // White noise: every band reads its share of the total power
  std::vector<double> x(1024);
  uint32_t seed = 1;
  for(auto& v : x)
  {
    seed = seed * 1664525u + 1013904223u;
    v = (seed >> 8) / double(1 << 24) - 0.5;
  }
  a.analyse(x.data());
  const double bin = rate / 1024.;
  const double f = 100 * bin;
  // Ten bins wide, then a tenth of a bin: the level per Hz is comparable
  const double wide = a.bandPower(f - 5 * bin, f + 5 * bin) / (10 * bin);
  const double narrow = a.bandPower(f - 0.05 * bin, f + 0.05 * bin) / (0.1 * bin);
  CHECK(std::abs(10. * std::log10(narrow / wide)) < 10.);
  // A band of exactly one bin reads that bin, divided by the bandwidth
  const double one = a.bandPower(f - 0.5 * bin, f + 0.49 * bin);
  CHECK(one == Approx(a.power[100] / a.enbw()));
}

#include <Ui/Spectrum.hpp>

TEST_CASE("Spectrum: no allocation while processing but for the band lists", "[meters][spectrum]")
{
  using namespace Ui::Spectrum;
  Node node;
  node.prepare({.input_channels = 2, .frames = 256, .rate = rate});
  auto l = sine(1000., 0.5, 256. / rate);
  auto r = sine(3000., 0.25, 256. / rate);
  double* chans[2]{l.data(), r.data()};
  node.inputs.audio.samples = chans;
  node.inputs.audio.channels = 2;
  node.monitor->viewers = 1;
  node(256);
  node.monitor->ring.provision();

  int lists = 0;
  alloc_counter::scope counter;
  for(int i = 0; i < 400; i++)
  {
    node.outputs.bands.value.reset();
    const int before = counter.allocations();
    node(256);
    if(node.outputs.bands.value)
      lists++;
    else
      CHECK(counter.allocations() == before);
  }
  // Twenty analyses a second: every tenth tick of 256 frames
  CHECK(lists == Approx(40.).margin(1.));
  // The channel count changing calls prepare again: nothing is rebuilt
  const int before = counter.allocations();
  node.prepare({.input_channels = 1, .frames = 256, .rate = rate});
  CHECK(counter.allocations() == before);
}

#include <Ui/Loudness.hpp>

namespace
{
//! Feeds the loudness node `seconds` of a stereo 1 kHz sine at `dbfs` per
//! channel, in 512-frame ticks.
void feedLoudness(Ui::Loudness::Node& node, double dbfs, double seconds)
{
  const int block = 512;
  static std::vector<double> l(block), r(block);
  static double phase = 0;
  double* chans[2]{l.data(), r.data()};
  node.inputs.audio.samples = chans;
  node.inputs.audio.channels = 2;
  const double amp = from_db(dbfs);
  for(int done = 0; done < seconds * rate; done += block)
  {
    for(int i = 0; i < block; i++)
    {
      l[i] = r[i] = amp * std::sin(phase);
      phase += 2. * std::numbers::pi * 1000. / rate;
    }
    node(block);
  }
}
}

TEST_CASE("Loudness: EBU Tech 3341 and 3342 test signals", "[meters][loudness]")
{
  using namespace Ui::Loudness;
  {
    // 3341 case 1: stereo 1 kHz at -23 dBFS reads -23 LUFS everywhere
    Node node;
    node.prepare({.input_channels = 2, .frames = 512, .rate = rate});
    feedLoudness(node, -23., 20.);
    CHECK(node.readings.momentary == Approx(-23.).margin(0.1));
    CHECK(node.readings.short_term == Approx(-23.).margin(0.1));
    CHECK(node.readings.integrated == Approx(-23.).margin(0.1));
    CHECK(node.outputs.integrated.value == Approx(-23.).margin(0.1));
    // A sine's true peak is its peak
    CHECK(node.readings.true_peak == Approx(-23.).margin(0.2));
  }
  {
    // 3341 case 3: -36, -23, -36 dBFS for 10, 60, 10 s: the relative gate
    // leaves the quiet parts out
    Node node;
    node.prepare({.input_channels = 2, .frames = 512, .rate = rate});
    feedLoudness(node, -36., 10.);
    feedLoudness(node, -23., 60.);
    feedLoudness(node, -36., 10.);
    CHECK(node.readings.integrated == Approx(-23.).margin(0.1));
  }
  {
    // 3342 case 1: -20 then -30 dBFS for 20 s each: LRA 10 LU
    Node node;
    node.prepare({.input_channels = 2, .frames = 512, .rate = rate});
    feedLoudness(node, -20., 20.);
    feedLoudness(node, -30., 20.);
    CHECK(node.readings.range == Approx(10.).margin(1.));
  }
  {
    // A reset starts a new programme, from a spare state while one is ready
    Node node;
    node.prepare({.input_channels = 2, .frames = 512, .rate = rate});
    int requests = 0;
    std::shared_ptr<LoudnessMonitor> requested;
    node.worker.request = [&](std::shared_ptr<LoudnessMonitor> m) {
      requests++;
      requested = std::move(m);
    };
    feedLoudness(node, -10., 5.);
    ebur128_state* spare = node.monitor->spare[0].load();
    REQUIRE(spare);
    node.monitor->reset = true;
    feedLoudness(node, -30., 10.);
    CHECK(node.state.get() == spare);
    CHECK(node.readings.integrated == Approx(-30.).margin(0.1));
    CHECK(node.monitor->retired[0].load() != nullptr);
    // The worker is asked once to rebuild what the reset used
    CHECK(requests == 1);
    REQUIRE(requested == node.monitor);
    Node::worker_t::work(requested);
    CHECK(node.monitor->retired[0].load() == nullptr);
    CHECK(node.monitor->spare[0].load() != nullptr);
    CHECK(node.monitor->spare[1].load() != nullptr);
    CHECK(!node.monitor->recycling);
  }
  {
    // Without a spare state, the reset waits for one: nothing is measured
    // meanwhile, and nothing allocated
    Node node;
    node.prepare({.input_channels = 2, .frames = 512, .rate = rate});
    int requests = 0;
    node.worker.request = [&](std::shared_ptr<LoudnessMonitor>) { requests++; };
    feedLoudness(node, -10., 3.);
    for(auto& slot : node.monitor->spare)
      if(auto* st = slot.exchange(nullptr))
        ebur128_destroy(&st);
    ebur128_state* before = node.state.get();
    const double duration = node.readings.duration;
    {
      alloc_counter::scope counter;
      node.inputs.reset.value.emplace();
      feedLoudness(node, -30., 1.);
      node.inputs.reset.value.reset();
      CHECK(counter.allocations() == 0);
    }
    CHECK(node.state.get() == before);
    CHECK(node.readings.duration == duration);
    CHECK(node.reset_pending);
    CHECK(requests == 1);

    Node::worker_t::work(node.monitor);
    feedLoudness(node, -30., 10.);
    CHECK(!node.reset_pending);
    CHECK(node.state.get() != before);
    CHECK(node.readings.integrated == Approx(-30.).margin(0.1));
  }
  {
    // The channel count changing calls prepare again: the measurement goes on
    Node node;
    node.prepare({.input_channels = 2, .frames = 512, .rate = rate});
    feedLoudness(node, -20., 5.);
    ebur128_state* st = node.state.get();
    node.prepare({.input_channels = 1, .frames = 512, .rate = rate});
    CHECK(node.state.get() == st);
    CHECK(node.readings.integrated == Approx(-20.).margin(0.1));
  }
}

TEST_CASE("Loudness: no allocation while processing", "[meters][loudness]")
{
  using namespace Ui::Loudness;
  Node node;
  node.prepare({.input_channels = 2, .frames = 512, .rate = rate});
  node.monitor->viewers = 1;
  feedLoudness(node, -20., 1.);
  alloc_counter::scope counter;
  feedLoudness(node, -20., 5.);
  CHECK(counter.allocations() == 0);
}

TEST_CASE("Loudness: BS.1770 channel weights", "[meters][loudness]")
{
  using namespace Ui::Monitor;
  // 5.1: surrounds at 110 degrees weigh 1.41, the LFE nothing
  CHECK(loudnessWeight(ChannelLayout::Surround51, 6, 4) == 1.41);
  CHECK(loudnessWeight(ChannelLayout::Surround51, 6, 3) == 0.);
  CHECK(loudnessWeight(ChannelLayout::Surround51, 6, 0) == 1.);
  // 7.1: sides at 90 degrees weigh 1.41, rears at 150 degrees 1
  CHECK(loudnessWeight(ChannelLayout::Surround71, 8, 4) == 1.41);
  CHECK(loudnessWeight(ChannelLayout::Surround71, 8, 6) == 1.);
  // Heights at 45 degrees weigh 1
  CHECK(loudnessWeight(ChannelLayout::Surround714, 12, 8) == 1.);
  // Ambisonics: W only
  CHECK(loudnessWeight(ChannelLayout::Ambisonic, 4, 0) == 1.);
  CHECK(loudnessWeight(ChannelLayout::Ambisonic, 4, 1) == 0.);
  CHECK(loudnessWeight(ChannelLayout::Ambisonic, 4, 3) == 0.);
}

#include <Ui/AudioScope.hpp>

TEST_CASE("Scope: the period of quiet tones, between samples", "[meters][scope]")
{
  using namespace Ui::AudioScope;
  for(double amp : {0.5, 0.01, 0.001})
  {
    std::vector<float> x(8192);
    for(std::size_t i = 0; i < x.size(); i++)
      x[i] = float(amp * std::sin(2. * std::numbers::pi * 441. * i / rate) + 0.1 * amp);
    CHECK(estimatePeriod(x.data(), int(x.size())) == Approx(rate / 441.).margin(0.05));
  }
  std::vector<float> silence(4096, 0.f);
  CHECK(estimatePeriod(silence.data(), int(silence.size())) == 0.);
}
