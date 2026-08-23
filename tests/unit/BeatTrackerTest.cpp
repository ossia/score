// Unit tests for the Beat Tracker process (AvndProcesses/BeatTracker.hpp):
// - the ODF runs on its own fixed hop, independent of the host block size
// - onset timing against a synthetic click train
// - tempo lock on synthetic metronomes, including near the octave boundary
// - DLL convergence, monotonicity and continuity
// - the error pre-filter's effect on a noisy error sequence
// - hold / re-lock behaviour across a silence
// - tap tempo averaging, the 2000 ms discard and the 1/12 BPM rounding

#include <AvndProcesses/BeatTracker.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdint>
#include <vector>

using Catch::Approx;
using namespace avnd_tools;

namespace
{
// Deterministic noise
struct lcg
{
  uint64_t s{0x243F6A8885A308D3ULL};
  double operator()()
  {
    s = s * 6364136223846793005ULL + 1442695040888963407ULL;
    return double(int64_t(s >> 11)) / double(1LL << 52); // ~[-1, 1]
  }
};

// Percussive metronome: exponentially decaying noise bursts at the given BPM.
std::vector<double> make_metronome(
    double bpm, double rate, double seconds, double start_offset = 0.05)
{
  std::vector<double> sig(size_t(seconds * rate), 0.);
  lcg rng;
  const double period = 60. / bpm;
  for(double t = start_offset; t < seconds; t += period)
  {
    const size_t start = size_t(t * rate);
    for(size_t i = 0; i < 1024 && start + i < sig.size(); i++)
      sig[start + i] += 0.8 * rng() * std::exp(-double(i) / 256.);
  }
  return sig;
}

// Drives the full process with a given host block size and records outputs.
struct harness
{
  BeatTracker fx;
  double rate{48000.};
  int block{512};
  int64_t sample_pos{0};
  double quarters{0.};
  std::vector<int64_t> beats, onsets;

  explicit harness(int block_size = 512, bool transport_hint = false)
      : block{block_size}
  {
    fx.inputs.hint.value = transport_hint;
    fx.prepare(halp::setup{
        .input_channels = 1, .output_channels = 0, .frames = block, .rate = rate});
    fx.outputs.beat.call.context = this;
    fx.outputs.beat.call.function = +[](void* ctx, int64_t ts) {
      auto& self = *(harness*)ctx;
      self.beats.push_back(self.sample_pos + ts);
    };
    fx.outputs.onset.call.context = this;
    fx.outputs.onset.call.function = +[](void* ctx, int64_t ts) {
      auto& self = *(harness*)ctx;
      self.onsets.push_back(self.sample_pos + ts);
    };
  }

  void run(const std::vector<double>& sig)
  {
    std::vector<double> buf(block);
    double* chans[1] = {buf.data()};
    for(size_t pos = 0; pos + block <= sig.size(); pos += block)
    {
      std::copy_n(sig.data() + pos, block, buf.data());
      fx.inputs.audio.samples = chans;
      fx.inputs.audio.channels = 1;

      halp::tick_flicks tk{};
      tk.frames = block;
      tk.tempo = 120.;
      tk.signature.num = 4;
      tk.signature.denom = 4;
      tk.position_in_frames = sample_pos;
      tk.start_position_in_quarters = quarters;
      quarters += (block / rate) * 2.; // 120 BPM
      tk.end_position_in_quarters = quarters;
      fx(tk);
      sample_pos += block;
    }
  }
};
}

TEST_CASE(
    "the ODF runs on a fixed internal hop, independent of the host block size",
    "[beattracker][odf]")
{
  const double rate = 48000.;
  const auto sig = make_metronome(120., rate, 3.);
  std::vector<float> sigf(sig.begin(), sig.end());

  auto run_with_blocks = [&](int bs) {
    btrk::spectral_flux_odf odf;
    odf.configure(rate);
    std::vector<double> frames;
    std::vector<int64_t> stamps;
    for(size_t pos = 0; pos < sigf.size(); pos += bs)
    {
      const int n = int(std::min<size_t>(bs, sigf.size() - pos));
      odf.process(sigf.data() + pos, n, [&](double v, int64_t s) {
        frames.push_back(v);
        stamps.push_back(s);
      });
    }
    return std::make_pair(frames, stamps);
  };

  const auto [f64, s64] = run_with_blocks(64);
  const auto [f256, s256] = run_with_blocks(256);
  const auto [f1024, s1024] = run_with_blocks(1024);

  REQUIRE(f64.size() > 500);
  REQUIRE(f64 == f256);   // bit-exact: same samples, same op order
  REQUIRE(f64 == f1024);
  REQUIRE(s64 == s256);
  REQUIRE(s64 == s1024);

  // the hop itself: ~200 fps whatever the audio settings say
  btrk::spectral_flux_odf odf;
  odf.configure(48000.);
  CHECK(odf.hop == 240);
  odf.configure(44100.);
  CHECK(odf.hop == 221);
  CHECK(odf.fps == Approx(200.).margin(1.));
}

TEST_CASE("onset timing on a synthetic click train", "[beattracker][onset]")
{
  harness h{512, false};
  const double rate = h.rate;
  const auto sig = make_metronome(120., rate, 4.); // clicks every 0.5 s
  h.run(sig);

  REQUIRE(h.onsets.size() >= 6);
  // each onset must sit at a constant latency from its click; measure that
  // latency on the first onset and assert the others deviate < 15 ms from it
  const double period = 0.5 * rate;
  auto nearest_click_error = [&](int64_t s) {
    const double k = std::round((s / period) - (0.05 / 0.5));
    const double click = (0.05 + 0.5 * k) * rate;
    return (s - click) / rate;
  };
  const double lat0 = nearest_click_error(h.onsets.front());
  for(auto s : h.onsets)
  {
    const double lat = nearest_click_error(s);
    CHECK(std::abs(lat - lat0) < 0.015);
    CHECK(std::abs(lat) < 0.06); // total latency stays bounded
  }
}

TEST_CASE("tempo locks on a synthetic metronome", "[beattracker][tempo]")
{
  // includes 158: near the top of the default 80-160 range, where octave
  // folding errors would show up first
  for(double bpm : {100., 128., 158.})
  {
    DYNAMIC_SECTION("bpm " << bpm)
    {
      harness h{512, false};
      const auto sig = make_metronome(bpm, h.rate, 25.);
      h.run(sig);

      INFO("tracked tempo: " << h.fx.outputs.tempo.value);
      CHECK(h.fx.outputs.valid.value);
      CHECK(std::abs(h.fx.outputs.tempo.value - bpm) / bpm < 0.03);
      CHECK(h.beats.size() > 10);
    }
  }
}

TEST_CASE("tempo lock is not disturbed by the host block size", "[beattracker][tempo]")
{
  for(int block : {64, 1024})
  {
    DYNAMIC_SECTION("block " << block)
    {
      harness h{block, false};
      const auto sig = make_metronome(128., h.rate, 25.);
      h.run(sig);
      CHECK(std::abs(h.fx.outputs.tempo.value - 128.) / 128. < 0.03);
    }
  }
}

TEST_CASE("DLL converges and stays monotonic and continuous", "[beattracker][dll]")
{
  btrk::beat_dll dll;
  dll.bw = 0.5;
  const double true_period = 0.55;
  dll.seed(1.0, 0.5); // wrong by 10%: must converge

  lcg rng;
  std::vector<double> emitted;
  double next_true_beat = 1.0 + true_period;
  double next_obs = next_true_beat - true_period / 2.;
  double prev_phase = dll.phase(0.95); // the clock is already mid-cycle
  double prev_t1 = dll.t1;
  bool had_roll = false;

  for(double now = 0.95; now < 40.; now += 0.005)
  {
    if(now >= next_obs)
    {
      dll.observe(next_true_beat + 0.003 * rng(), now);
      next_true_beat += true_period;
      next_obs = next_true_beat - true_period / 2.;
    }
    const double t1_before = dll.t1;
    had_roll = false;
    dll.advance(now, [&](double t, int64_t) {
      emitted.push_back(t);
      had_roll = true;
    });
    // the pending-beat time only moves forward through advance()
    REQUIRE(dll.t1 >= t1_before);

    const double ph = dll.phase(now);
    REQUIRE(ph >= 0.);
    REQUIRE(ph <= 1.);
    if(!had_roll)
    {
      // within a cycle the phase never jumps: small bounded increments
      REQUIRE(ph - prev_phase > -0.35); // corrections may stretch the cycle
      REQUIRE(ph - prev_phase < 0.35);
    }
    prev_phase = ph;
    prev_t1 = dll.t1;
  }

  // convergence: the filtered period reaches the true one
  CHECK(std::abs(dll.e2 - true_period) / true_period < 0.02);
  // emitted beat times strictly increasing
  REQUIRE(emitted.size() > 30);
  for(size_t i = 1; i < emitted.size(); i++)
    REQUIRE(emitted[i] > emitted[i - 1]);
  // and spaced about one true period apart at the end
  const auto n = emitted.size();
  CHECK(emitted[n - 1] - emitted[n - 2] == Approx(true_period).epsilon(0.05));
}

TEST_CASE("the error pre-filter attenuates noise on the error value",
    "[beattracker][dll]")
{
  // the LAC2012 fix: a second-order low-pass at 20x the loop bandwidth ahead
  // of the loop filter, so error noise does not phase-modulate the clock
  const double bw = 0.05, period = 0.5;
  const double alpha = ossia::lowpass_alpha(20. * bw, period);
  ossia::one_pole_filter<double> f1, f2;

  lcg rng;
  double var_raw = 0., var_filt = 0., mean_raw = 0., mean_filt = 0.;
  std::vector<double> raw, filt;
  for(int i = 0; i < 4000; i++)
  {
    const double e = 0.01 * rng();
    const double ef = f2(f1(e, alpha), alpha);
    raw.push_back(e);
    filt.push_back(ef);
  }
  for(auto v : raw)
    mean_raw += v;
  for(auto v : filt)
    mean_filt += v;
  mean_raw /= raw.size();
  mean_filt /= filt.size();
  for(auto v : raw)
    var_raw += (v - mean_raw) * (v - mean_raw);
  for(auto v : filt)
    var_filt += (v - mean_filt) * (v - mean_filt);

  CHECK(var_filt < 0.6 * var_raw);
}

TEST_CASE("hold across a silence, then re-lock", "[beattracker][monitor]")
{
  harness h{512, false};
  const double rate = h.rate;

  auto a = make_metronome(120., rate, 12.);
  h.run(a);
  REQUIRE(h.fx.outputs.valid.value);
  const double locked_tempo = h.fx.outputs.tempo.value;
  REQUIRE(std::abs(locked_tempo - 120.) / 120. < 0.03);
  const auto beats_before = h.beats.size();
  const auto index_before = h.fx.outputs.beat_index.value;

  // 3 s of silence: valid drops, the tempo holds, the clock keeps running
  std::vector<double> silence(size_t(3. * rate), 0.);
  h.run(silence);
  CHECK(!h.fx.outputs.valid.value);
  CHECK(std::abs(h.fx.outputs.tempo.value - locked_tempo) < 0.5);
  CHECK(h.fx.outputs.beat_index.value > index_before); // free-runs, no stall
  CHECK(h.beats.size() > beats_before);                // beats keep coming

  // audio returns: it must become valid and re-lock
  auto b = make_metronome(120., rate, 10.);
  h.run(b);
  CHECK(h.fx.outputs.valid.value);
  CHECK(std::abs(h.fx.outputs.tempo.value - 120.) / 120. < 0.03);
}

TEST_CASE("tap tempo follows the Mixxx semantics", "[beattracker][tap]")
{
  btrk::tap_tempo tap;

  SECTION("regular taps at 0.5 s give 120 BPM")
  {
    double bpm = 0.;
    for(int i = 0; i < 8; i++)
      bpm = tap.tap(i * 0.5);
    CHECK(bpm == Approx(120.));
  }

  SECTION("the average is over the whole series")
  {
    // alternating 0.4 / 0.6 s: mean interval 0.5 s
    double t = 0., bpm = 0.;
    bpm = tap.tap(t);
    for(int i = 0; i < 10; i++)
    {
      t += (i % 2 ? 0.4 : 0.6);
      bpm = tap.tap(t);
    }
    CHECK(bpm == Approx(120.).margin(2.));
  }

  SECTION("an interval > 2000 ms discards the whole series")
  {
    for(int i = 0; i < 4; i++)
      tap.tap(i * 0.5);
    // long pause: series restarts
    CHECK(tap.tap(10.) == 0.);       // first tap of the new series
    CHECK(tap.tap(10.4) == Approx(150.));
  }

  SECTION("the result is rounded to 1/12 BPM")
  {
    // interval 0.495 s -> 121.212... BPM -> nearest multiple of 1/12
    double bpm = 0.;
    for(int i = 0; i < 6; i++)
      bpm = tap.tap(i * 0.495);
    CHECK(bpm == Approx(std::round((60. / 0.495) * 12.) / 12.).margin(1e-9));
    CHECK(std::abs(bpm * 12. - std::round(bpm * 12.)) < 1e-9);
  }

  SECTION("below 30 BPM is rejected")
  {
    CHECK(tap.tap(0.) == 0.);
    CHECK(tap.tap(1.99) > 0.);  // 1.99 s interval: ~30.15 BPM, allowed
    btrk::tap_tempo tap2;
    CHECK(tap2.tap(0.) == 0.);
    CHECK(tap2.tap(2.) == 30.); // exactly 30 BPM
  }
}

// Hidden diagnostic: run `test_unit_beat_tracker "[.debug]"` to trace the
// internal state second by second on a 100 BPM metronome.
TEST_CASE("debug 100bpm trace", "[.debug]")
{
  harness h{512, false};
  const auto sig = make_metronome(100., h.rate, 25.);
  const size_t chunk = size_t(h.rate);
  for(size_t s = 0; s + chunk <= sig.size(); s += chunk)
  {
    std::vector<double> part(sig.begin() + s, sig.begin() + s + chunk);
    h.run(part);
    printf(
        "t=%2zu est.period=%7.2f est.conf=%5.3f ct.beta=%7.2f dll.e2=%6.4f "
        "tempo=%7.2f locked=%d mon=%5.3f beats=%zu\n",
        s / chunk, h.fx.m_est.period, h.fx.m_est.confidence, h.fx.m_ct.beta,
        h.fx.m_dll.e2, h.fx.outputs.tempo.value, (int)h.fx.m_is_locked,
        h.fx.m_mon.combined, h.beats.size());
  }
  CHECK(true);
}

TEST_CASE("debug odf trace", "[.debug2]")
{
  const double rate = 48000.;
  const auto sig = make_metronome(100., rate, 25.);
  std::vector<float> sigf(sig.begin(), sig.end());

  btrk::spectral_flux_odf odf;
  odf.configure(rate);
  btrk::tempo_estimator est;
  est.configure(odf.fps, 80., 160.);

  ossia::moving_average_filter<double, 16> ma;
  std::vector<double> d;
  odf.process(sigf.data(), (int)sigf.size(), [&](double v, int64_t) {
    d.push_back(std::max(0., v - ma(v)));
    est.push(std::max(0., v - ma(v)));
  });
  printf("fps=%f frames=%zu est.period=%f conf=%f\n", odf.fps, d.size(), est.period,
      est.confidence);

  // peak positions of the detrended ODF
  std::vector<int> peaks;
  for(size_t i = 2; i + 1 < d.size(); i++)
    if(d[i - 1] > d[i] && d[i - 1] >= d[i - 2] && d[i - 1] > 0.05)
      if(peaks.empty() || (int)i - 1 - peaks.back() > 6)
        peaks.push_back((int)i - 1);
  printf("first peaks: ");
  for(size_t i = 0; i + 1 < peaks.size() && i < 12; i++)
    printf("%d ", peaks[i + 1] - peaks[i]);
  printf("\n");

  // top raw comb lags
  std::vector<std::pair<double, int>> sc;
  for(int l = est.min_lag; l <= est.max_lag; l++)
    sc.push_back({est.comb_raw[l], l});
  std::sort(sc.rbegin(), sc.rend());
  printf("top raw comb: ");
  for(int i = 0; i < 6; i++)
    printf("(%d: %.5f) ", sc[i].second, sc[i].first);
  printf("\n");
  CHECK(true);
}
