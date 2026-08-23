#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

// Real-time beat tracker / tempo follower.
//
// Architecture, four layers; only the oscillator talks to the outputs:
//
//   audio -> [ODF]  log-filtered spectral flux (SuperFlux-style), running on
//                   its OWN fixed hop (~200 fps), decoupled from the host
//                   block size by an input ring buffer.
//        -> [EST]   BTrack-style tempo estimator: ACF + shifted comb filter
//                   bank + Rayleigh / two-state context prior, updated a few
//                   times per second. User Min/Max BPM range instead of the
//                   traditional hard-coded 80-160 octave.
//        -> [OSC]   cumulative-score beat prediction (Stark DAFx-09 momentum
//                   extrapolation) feeding a second-order delay-locked loop
//                   (Adriaensen LAC2005/LAC2012): filtered next-beat time and
//                   filtered period, continuous and queryable at any sample.
//        + [MON]    confidence monitor (IBT-style 3 s window / 1 s hop)
//                   driving the loop bandwidth ladder, auto-hold and
//                   re-induction.
//
// Output shaping: a rate, not a position (phase corrected through the rate,
// Mixxx-style caps), future event times (Next beat + Lookahead), and origin
// tracking so the follower does not chase tempo changes it caused itself.
//
// The output triple {Timecode-ish phase, Speed, Valid} is shaped to drop into
// TimecodeSynchronizer's {Timecode, Speed, Validity} inlets, and Speed can
// drive the interval Speed inlet directly.

#include <ossia/audio/fft.hpp>
#include <ossia/detail/math.hpp>
#include <ossia/math/filters.hpp>
#include <ossia/network/value/value.hpp>

#include <halp/audio.hpp>
#include <halp/callback.hpp>
#include <halp/controls.hpp>
#include <halp/layout.hpp>
#include <halp/meta.hpp>
#include <halp/sample_accurate_controls.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <vector>

namespace avnd_tools
{
namespace btrk
{

static constexpr double default_min_bpm = 80.;
static constexpr double default_max_bpm = 160.;
static constexpr double absolute_min_bpm = 30.;
static constexpr double absolute_max_bpm = 300.;

//
// [ODF] Log-filtered spectral flux with a maximum filter across frequency
// (SuperFlux, Boeck & Widmer DAFx-13), on a fixed internal hop.
//
// - ~200 frames per second whatever the host buffer size: the input is ring-
//   buffered and frames are cut every `hop` samples. (GistState gets this
//   wrong: its frame rate follows the audio settings.)
// - log-magnitude on a quarter-tone triangular filterbank: gain-robust, since
//   log(a) - log(b) is a ratio (Boeck ISMIR 2012, LogFiltSpecFlux).
// - difference against frame n-2 with a 3-bin maximum filter across frequency
//   only, strictly causal (SuperFlux mu=2).
// - optional adaptive whitening (Stowell): P(n,k) = max(|S|, r, m*P(n-1,k)).
// - band emphasis: the flux is summed with per-band weights so the tracker
//   can listen to the kick, the snare, transients, or everything.
//
struct spectral_flux_odf
{
  enum class band_mode
  {
    full,
    kick,      // ~30-120 Hz
    snare,     // ~150-400 Hz
    transient, // ~2-5 kHz
    kick_snare
  };

  struct filter_band
  {
    int first_bin{};
    std::vector<float> weights; // triangular, normalized to sum 1
    float center_hz{};
  };

  double rate{};
  int fft_size{};
  int hop{};
  double fps{}; // rate / hop, ~200

  ossia::fft fft{16};
  std::vector<float> window;       // Hann
  std::vector<float> ring;         // input ring, fft_size samples
  int ring_pos{};                  // write position
  int hop_fill{};                  // samples since last frame
  int64_t total_samples{};         // absolute sample clock of the last sample
  int64_t frame_count{};           // ODF frames emitted so far

  std::vector<float> mags;         // |S|, fft_size/2+1
  std::vector<float> whitening_peaks;
  bool whitening{false};
  float whitening_floor{0.01f};
  float whitening_relax{};         // per-frame memory coefficient

  std::vector<filter_band> bands;
  std::vector<float> band_weights; // band emphasis, [0,1] per band
  // last 3 filterbank frames (log domain), ring of 3
  std::vector<float> band_frames;  // 3 * bands.size()
  int band_frame_head{};
  int band_frames_filled{};

  band_mode mode{band_mode::full};

  void configure(double sample_rate)
  {
    rate = sample_rate;
    fft_size = rate > 50000. ? 4096 : 2048;
    hop = std::max(1, (int)std::lround(rate / 200.));
    fps = rate / hop;

    fft.reset(fft_size);
    window.resize(fft_size);
    for(int i = 0; i < fft_size; i++)
      window[i] = 0.5f - 0.5f * std::cos(2.f * float(M_PI) * i / float(fft_size - 1));

    ring.assign(fft_size, 0.f);
    ring_pos = 0;
    hop_fill = 0;
    total_samples = 0;
    frame_count = 0;

    mags.assign(fft_size / 2 + 1, 0.f);
    whitening_peaks.assign(fft_size / 2 + 1, whitening_floor);
    // 60 dB of relaxation over 25.6 s (Stowell's defaults)
    whitening_relax = std::pow(10.f, -3.f / float(25.6 * fps));

    build_filterbank();
    band_frames.assign(3 * bands.size(), 0.f);
    band_frame_head = 0;
    band_frames_filled = 0;
    set_band_mode(mode);
  }

  void build_filterbank()
  {
    bands.clear();
    const double fmax = std::min(17000., rate * 0.45);
    const double bin_hz = rate / double(fft_size);

    // quarter-tone spaced center frequencies, deduplicated per bin
    std::vector<double> centers;
    int last_bin = -1;
    for(int i = 0;; i++)
    {
      const double f = 27.5 * std::pow(2., i / 24.);
      if(f > fmax)
        break;
      const int b = (int)std::lround(f / bin_hz);
      if(b != last_bin && b >= 1)
      {
        centers.push_back(f);
        last_bin = b;
      }
    }
    if(centers.size() < 3)
      return;

    for(std::size_t i = 1; i + 1 < centers.size(); i++)
    {
      const double lo = centers[i - 1] / bin_hz;
      const double c = centers[i] / bin_hz;
      const double hi = centers[i + 1] / bin_hz;
      filter_band band;
      band.center_hz = float(centers[i]);
      band.first_bin = std::max(1, (int)std::ceil(lo));
      const int end_bin = std::min(fft_size / 2, (int)std::floor(hi));
      if(end_bin < band.first_bin)
        continue;
      band.weights.resize(end_bin - band.first_bin + 1);
      float sum = 0.f;
      for(int b = band.first_bin; b <= end_bin; b++)
      {
        double w = b <= c ? (b - lo) / std::max(1e-9, c - lo)
                          : (hi - b) / std::max(1e-9, hi - c);
        w = std::max(0., w);
        band.weights[b - band.first_bin] = float(w);
        sum += float(w);
      }
      if(sum <= 0.f)
        continue;
      for(auto& w : band.weights)
        w /= sum;
      bands.push_back(std::move(band));
    }
  }

  void set_band_mode(band_mode m)
  {
    mode = m;
    band_weights.assign(bands.size(), 0.f);
    auto in = [](float f, float lo, float hi) { return f >= lo && f <= hi; };
    for(std::size_t i = 0; i < bands.size(); i++)
    {
      const float f = bands[i].center_hz;
      switch(m)
      {
        case band_mode::full:
          band_weights[i] = 1.f;
          break;
        case band_mode::kick:
          band_weights[i] = in(f, 30.f, 120.f) ? 1.f : 0.f;
          break;
        case band_mode::snare:
          band_weights[i] = in(f, 150.f, 400.f) ? 1.f : 0.f;
          break;
        case band_mode::transient:
          band_weights[i] = in(f, 2000.f, 5000.f) ? 1.f : 0.f;
          break;
        case band_mode::kick_snare:
          band_weights[i] = (in(f, 30.f, 120.f) || in(f, 150.f, 400.f)) ? 1.f : 0.f;
          break;
      }
    }
  }

  // Push n mono samples; calls on_frame(odf_value, end_sample_index) for every
  // completed hop. end_sample_index is the absolute index of the sample just
  // after the frame, on the same clock as total_samples.
  template <typename F>
  void process(const float* in, int n, F&& on_frame)
  {
    for(int i = 0; i < n; i++)
    {
      ring[ring_pos] = in[i];
      ring_pos = ring_pos + 1 == fft_size ? 0 : ring_pos + 1;
      total_samples++;
      if(++hop_fill >= hop)
      {
        hop_fill = 0;
        on_frame(compute_frame(), total_samples);
      }
    }
  }

  double compute_frame()
  {
    // time-order the ring into the FFT input, windowed
    auto* input = fft.input();
    int idx = ring_pos; // oldest sample
    for(int i = 0; i < fft_size; i++)
    {
      input[i] = ring[idx] * window[i];
      idx = idx + 1 == fft_size ? 0 : idx + 1;
    }
    auto* out = fft.execute();

    const int nbins = fft_size / 2 + 1;
    const float norm = 2.f / float(fft_size);
    for(int b = 0; b < nbins; b++)
    {
      const float re = float(out[b][0]), im = float(out[b][1]);
      float m = std::sqrt(re * re + im * im) * norm;
      if(whitening)
      {
        float& p = whitening_peaks[b];
        p = std::max({m, whitening_floor, p * whitening_relax});
        m = m / p;
      }
      mags[b] = m;
    }

    const int nb = (int)bands.size();
    if(nb == 0)
      return 0.;
    float* cur = band_frames.data() + band_frame_head * nb;
    for(int k = 0; k < nb; k++)
    {
      const auto& band = bands[k];
      float acc = 0.f;
      for(std::size_t j = 0; j < band.weights.size(); j++)
        acc += band.weights[j] * mags[band.first_bin + j];
      // log compression: gain-robust flux
      cur[k] = std::log10(1.f + 20.f * acc);
    }

    double flux = 0.;
    if(band_frames_filled >= 2)
    {
      // frame n-2, with a 3-bin maximum filter across frequency (causal:
      // only the past frame is widened)
      const int prev_head = (band_frame_head + 1) % 3; // oldest of the 3
      const float* prev = band_frames.data() + prev_head * nb;
      for(int k = 0; k < nb; k++)
      {
        if(band_weights[k] <= 0.f)
          continue;
        float ref = prev[k];
        if(k > 0)
          ref = std::max(ref, prev[k - 1]);
        if(k + 1 < nb)
          ref = std::max(ref, prev[k + 1]);
        const float d = cur[k] - ref;
        if(d > 0.f)
          flux += band_weights[k] * d;
      }
    }

    band_frame_head = (band_frame_head + 1) % 3;
    if(band_frames_filled < 3)
      band_frames_filled++;
    frame_count++;
    return flux;
  }
};

//
// [EST] Tempo estimator: detrended ODF window -> autocorrelation -> shifted
// comb filter bank -> prior -> best beat period. (Davies & Plumbley / Stark
// BTrack lineage.) Updated every `update_interval` ODF frames.
//
struct tempo_estimator
{
  double fps{200.};
  int min_lag{75}, max_lag{150}; // from the user BPM range
  static constexpr int window_size = 1024; // ~5 s at 200 fps
  static constexpr int update_interval = 128;

  std::vector<double> window; // ring of detrended ODF
  int head{};
  int filled{};
  int since_update{};

  std::vector<double> scratch; // linearized window
  std::vector<double> acf;
  std::vector<double> comb;      // prior-weighted
  std::vector<double> comb_raw;  // prior-free, for the confidence margin

  // two-state context (Davies): after three consistent estimates, swap the
  // Rayleigh prior for a tight Gaussian around the previous period, which is
  // what prevents on-beat -> off-beat switches.
  double history[3]{};
  int history_n{};
  bool context_locked{};
  double context_period{};
  int context_disagreements{};

  double period{0.};     // best period, in ODF frames; 0 = none yet
  double confidence{0.}; // margin between best and second-best hypothesis

  void configure(double frames_per_second, double min_bpm, double max_bpm)
  {
    fps = frames_per_second;
    min_bpm = std::clamp(min_bpm, absolute_min_bpm, absolute_max_bpm);
    max_bpm = std::clamp(max_bpm, min_bpm + 1., absolute_max_bpm);
    min_lag = std::max(4, (int)std::lround(60. * fps / max_bpm));
    max_lag = std::min(window_size / 2, (int)std::lround(60. * fps / min_bpm));
    if(max_lag <= min_lag)
      max_lag = min_lag + 1;
    if(window.empty())
    {
      window.assign(window_size, 0.);
      scratch.resize(window_size);
      acf.resize(window_size);
      comb.resize(window_size);
      comb_raw.resize(window_size);
    }
  }

  void reset_context()
  {
    history_n = 0;
    context_locked = false;
    context_disagreements = 0;
  }

  // Transport / tap seeding: pretend we already saw a consistent period.
  void seed(double period_frames)
  {
    period_frames = std::clamp<double>(period_frames, min_lag, max_lag);
    period = period_frames;
    context_period = period_frames;
    context_locked = true;
    history_n = 0;
  }

  // returns true when a new estimate was produced
  bool push(double detrended_odf)
  {
    window[head] = detrended_odf;
    head = head + 1 == window_size ? 0 : head + 1;
    if(filled < window_size)
      filled++;
    // Wait for a full analysis window: estimates from a half-empty window are
    // mostly prior, and the two-state context must never lock onto those.
    if(++since_update < update_interval || filled < window_size)
      return false;
    since_update = 0;
    estimate();
    return true;
  }

  void estimate()
  {
    const int n = filled;
    // linearize, oldest first
    int idx = (head - n + window_size) % window_size;
    for(int i = 0; i < n; i++)
    {
      scratch[i] = window[idx];
      idx = idx + 1 == window_size ? 0 : idx + 1;
    }

    // autocorrelation, normalized per-lag by the overlap length
    const int max_acf_lag = std::min(n - 1, 4 * max_lag + 4);
    for(int lag = 1; lag <= max_acf_lag; lag++)
    {
      double acc = 0.;
      for(int i = lag; i < n; i++)
        acc += scratch[i] * scratch[i - lag];
      acf[lag] = acc / double(n - lag);
    }

    // shifted comb filter bank with 4 harmonics
    double best = 0., best_lag = 0.;
    double best_raw = 0., best_raw_lag = 0.;
    for(int lag = min_lag; lag <= max_lag; lag++)
    {
      double sc = 0.;
      int harmonics = 0;
      for(int a = 1; a <= 4; a++)
      {
        const int c = a * lag;
        if(c + (a - 1) > max_acf_lag)
          break;
        double h = 0.;
        for(int b = -(a - 1); b <= a - 1; b++)
          h += acf[c + b];
        sc += h / double(2 * a - 1);
        harmonics++;
      }
      if(harmonics == 0)
        continue;
      sc = sc / harmonics;
      comb_raw[lag] = sc;
      if(sc > best_raw)
      {
        best_raw = sc;
        best_raw_lag = lag;
      }
      sc *= prior(lag);
      comb[lag] = sc;
      if(sc > best)
      {
        best = sc;
        best_lag = lag;
      }
    }
    if(best <= 0. || best_raw <= 0.)
    {
      confidence = 0.;
      return;
    }

    // Two-state context sanity check: the tight Gaussian prior can only ever
    // confirm itself, so verify it against the prior-free evidence. Three
    // consecutive disagreements with the raw best hypothesis mean the context
    // was locked onto the wrong period: drop it and re-induce.
    if(context_locked)
    {
      double d = std::abs(best_raw_lag - context_period);
      // octave-tolerant: agreeing with double/half the period is agreement
      d = std::min({d, std::abs(2. * best_raw_lag - context_period),
                    std::abs(0.5 * best_raw_lag - context_period)});
      if(d > 0.12 * context_period)
      {
        if(++context_disagreements >= 3)
        {
          reset_context();
          period = best_raw_lag;
          best_lag = best_raw_lag;
        }
      }
      else
      {
        context_disagreements = 0;
      }
    }

    // parabolic interpolation around the best integer lag
    int bl = (int)best_lag;
    if(bl > min_lag && bl < max_lag)
    {
      const double ym = comb[bl - 1], y0 = comb[bl], yp = comb[bl + 1];
      const double denom = ym - 2 * y0 + yp;
      if(std::abs(denom) > 1e-12)
        best_lag = bl + std::clamp(0.5 * (ym - yp) / denom, -0.5, 0.5);
    }

    // hypothesis margin: best vs the best sufficiently-distant competitor, on
    // the prior-free comb - measured on the prior-weighted one, a wrongly
    // locked context prior would suppress every competitor and make the
    // wrong answer look certain.
    const double base = comb_raw[std::clamp((int)std::lround(best_lag), min_lag, max_lag)];
    double second = 0.;
    for(int lag = min_lag; lag <= max_lag; lag++)
    {
      if(std::abs(lag - best_lag) < 0.15 * best_lag)
        continue;
      second = std::max(second, comb_raw[lag]);
    }
    confidence = base > 0. ? std::clamp(1. - second / base, 0., 1.) : 0.;
    period = best_lag;

    // two-state context switch, after three consistent beat periods
    history[history_n % 3] = best_lag;
    history_n++;
    if(context_locked)
    {
      // track slowly, so the prediction context follows drift without
      // being able to run away within one update
      context_period += 0.2 * (best_lag - context_period);
    }
    else if(history_n >= 3)
    {
      const double a = history[0], b = history[1], c = history[2];
      const double mx = std::max({a, b, c}), mn = std::min({a, b, c});
      if(mx - mn < 0.075 * mx)
      {
        context_locked = true;
        context_period = (a + b + c) / 3.;
      }
    }
  }

  double prior(int lag) const
  {
    if(context_locked)
    {
      // tight Gaussian around the previous prediction, sigma^2 = tau/8
      const double s2 = std::max(1., context_period / 8.);
      const double d = lag - context_period;
      return std::exp(-d * d / (2. * s2));
    }
    // Rayleigh, mode at ~107.6 BPM (b = 48 samples at BTrack's 86 fps,
    // rescaled to our frame rate)
    const double b = 0.5585 * fps;
    const double l = lag;
    return (l / (b * b)) * std::exp(-l * l / (2. * b * b));
  }
};

//
// [OSC-1] Cumulative beat score + causal prediction (Stark DAFx-09):
//   C(m) = (1-a)*D(m) + a*max_v(W1(v) * C(m+v)),  v in [-2b, -b/2]
// and at the fixed instant m0 = last predicted beat + b/2, extrapolate C one
// period forward with a=1 (pure momentum, no ODF) and pick
//   next = m0 + argmax_{v=1..b}( Cf(m0+v) * W2(v) )
// so the beat is emitted before the audio evidence arrives.
//
struct cumulative_tracker
{
  static constexpr double alpha = 0.9;
  static constexpr double eta = 5.;

  std::vector<float> cs; // ring
  int cs_size{4096};
  int64_t frame{};       // current ODF frame index
  double beta{100.};     // beat period in ODF frames
  std::vector<float> w1; // transition weights for v in [-2b, -b/2]
  int w1_lo{}, w1_hi{};  // v range: v = -(w1_lo + i), i.e. [-2b .. -b/2]
  std::vector<float> future;

  int64_t next_prediction_frame{-1};
  int64_t last_beat_frame{-1};
  double last_contrast{};

  void configure()
  {
    cs.assign(cs_size, 0.f);
    frame = 0;
    next_prediction_frame = -1;
    last_beat_frame = -1;
    future.resize(cs_size + 1024);
    set_period(beta);
  }

  void set_period(double period_frames)
  {
    beta = std::clamp(period_frames, 8., 800.);
    const int lo = std::max(1, (int)std::lround(beta / 2.));
    const int hi = std::min(cs_size - 1, (int)std::lround(2. * beta));
    w1_lo = lo;
    w1_hi = hi;
    w1.resize(hi - lo + 1);
    for(int v = lo; v <= hi; v++)
    {
      const double r = eta * std::log(double(v) / beta);
      w1[v - lo] = float(std::exp(-0.5 * r * r));
    }
  }

  float& at(int64_t m) { return cs[(m % cs_size + cs_size) % cs_size]; }

  // returns predicted next-beat frame when a new prediction was made, else -1
  int64_t push(double detrended_odf)
  {
    // C(m) = (1-a) * D(m) + a * max_v( W1(v) * C(m+v) )
    float cmax = 0.f;
    const int64_t lim = std::min<int64_t>(frame, w1_hi);
    for(int64_t v = w1_lo; v <= lim; v++)
    {
      const float c = at(frame - v) * w1[v - w1_lo];
      if(c > cmax)
        cmax = c;
    }
    at(frame) = float((1. - alpha) * detrended_odf + alpha * cmax);

    int64_t predicted = -1;
    if(next_prediction_frame < 0 && frame > int64_t(2 * beta))
      next_prediction_frame = frame + 1; // bootstrap
    if(frame == next_prediction_frame)
      predicted = predict();

    frame++;
    return predicted;
  }

  int64_t predict()
  {
    const int b = std::max(2, (int)std::lround(beta));
    const int past = std::min<int64_t>(frame, w1_hi + b + 1);
    // linearize the recent past, then extrapolate with alpha = 1
    for(int i = 0; i < past; i++)
      future[i] = at(frame - past + 1 + i);
    for(int i = 0; i < b + 1; i++)
    {
      float cmax = 0.f;
      const int base = past + i;
      for(int v = w1_lo; v <= w1_hi && v < base; v++)
      {
        const float c = future[base - v - 1] * w1[v - w1_lo];
        if(c > cmax)
          cmax = c;
      }
      future[base] = cmax; // pure momentum: no ODF term
    }

    // W2: Gaussian centered half a period ahead
    const double half = beta / 2.;
    double best = -1.;
    int best_v = b / 2 + 1;
    for(int v = 1; v <= b; v++)
    {
      const double d = v - half;
      const double sc = future[past + v] * std::exp(-d * d / (2. * half * half));
      if(sc > best)
      {
        best = sc;
        best_v = v;
      }
    }

    const int64_t beat_frame = frame + best_v;
    last_beat_frame = beat_frame;
    next_prediction_frame = beat_frame + std::max<int64_t>(1, (int64_t)half);

    // cumulative-score contrast for the confidence monitor: score at the
    // predicted beat against the mean magnitude over the last period
    double mean = 1e-9;
    const int n = std::min<int64_t>(frame, b);
    for(int i = 0; i < n; i++)
      mean += std::abs(at(frame - i));
    mean /= std::max(1, (int)n);
    const double contrast = future[past + best_v] / mean;
    last_contrast = std::clamp(contrast / (contrast + 1.5), 0., 1.);

    return beat_frame;
  }
};

//
// [OSC-2] Delay-locked loop on beat times (Adriaensen, "Using a DLL to filter
// time", LAC2005), with the LAC2012 improvements: a second-order low-pass on
// the error ahead of the loop filter at 20x the loop bandwidth, one-shot
// startup correction followed by ~4 s of elevated bandwidth, and restarts
// that reuse the previous rate estimate.
//
// States: t0 = previous beat time, t1 = filtered next beat time, e2 = filtered
// beat period, all in seconds. tempo = 60/e2 and phase(t) = (t-t0)/(t1-t0) are
// continuous and monotonic - the synchronizer's contract.
//
// A third-order variant (PipeWire-style extra integrator, z3) is offered for
// inputs with period drift.
//
struct beat_dll
{
  double t0{}, t1{}, e2{0.5};
  double z3{};
  double bw{1.0}; // loop bandwidth, Hz; 0 = hold (freeze e2, keep t1 running)
  int order{2};   // 2 or 3
  bool inited{};
  int64_t beat_index{-1};

  ossia::one_pole_filter<double> pf1{}, pf2{};
  double innovation_rms{}; // mean square of filtered innovations, seconds^2

  static constexpr double min_period = 60. / absolute_max_bpm;
  static constexpr double max_period = 60. / absolute_min_bpm;

  void reset()
  {
    inited = false;
    z3 = 0.;
    beat_index = -1;
    innovation_rms = 0.;
    pf1.reset();
    pf2.reset();
  }

  // Hard-correct in one shot (Adriaensen startup). A restart with
  // keep_rate = true reuses the previous period estimate for fast re-locking.
  void seed(double next_beat_time, double period, bool keep_rate = false)
  {
    if(!(keep_rate && inited))
      e2 = std::clamp(period, min_period, max_period);
    t1 = next_beat_time;
    t0 = t1 - e2;
    z3 = 0.;
    pf1.reset();
    pf2.reset();
    innovation_rms = 0.;
    inited = true;
  }

  // Observation: the next beat is expected at time tb (seconds). Updates the
  // filtered next-beat time and period. Never rolls the cycle: advance() does.
  void observe(double tb, double now)
  {
    if(!inited)
    {
      seed(tb, e2);
      return;
    }

    double e = tb - t1;
    // fold to the nearest beat so a phase-shifted observation nudges instead
    // of dragging the clock a whole period
    e -= e2 * std::round(e / e2);

    // LAC2012: second-order low-pass on the error, 20x the loop bandwidth,
    // sampled at the beat rate, to keep high-frequency error noise from
    // phase-modulating the clock.
    const double cutoff = std::max(0.05, 20. * bw);
    const double a = ossia::lowpass_alpha(cutoff, e2);
    const double ef = pf2(pf1(e, a), a);
    innovation_rms = 0.9 * innovation_rms + 0.1 * ef * ef;

    if(bw <= 0.)
      return; // hold: freeze both phase and period corrections

    // omega = 2*pi*B/F, F = beat rate (JACK2's derivation: coefficient from
    // the period, so the bandwidth is independent of the period). The
    // precedent DLLs update at the audio-period rate where B/F is tiny; here
    // the update rate is the beat rate, so omega must be clamped to stay in
    // the loop's stable region - beyond ~0.5 rad the per-beat corrections
    // exceed the error and the loop limit-cycles instead of converging.
    const double w = std::min(2. * M_PI * bw * e2, 0.5);
    double dphase, dperiod;
    if(order <= 2)
    {
      dphase = M_SQRT2 * w * ef;
      dperiod = w * w * ef;
    }
    else
    {
      dphase = 2. * w * ef;
      dperiod = 2. * w * w * ef + z3;
      z3 = std::clamp(z3 + w * w * w * ef, -0.02, 0.02);
    }

    // never correct more than a fraction of a beat inside one observation:
    // phase error is spread over the next beats (B-Keeper nudge semantics).
    // A correction towards `now` may consume at most half the remaining cycle
    // time - pulling t1 down to now would teleport the phase to 1 in one step.
    const double max_towards_now = t1 > now ? 0.5 * (t1 - now) : 0.;
    t1 += std::clamp(dphase, -std::min(0.125 * e2, max_towards_now), 0.125 * e2);
    e2 = std::clamp(e2 + std::clamp(dperiod, -0.05 * e2, 0.05 * e2), min_period,
                    max_period);

    // keep the clock sane: the pending beat stays in the future, and the
    // running cycle keeps a positive length
    if(t1 < now)
      t1 = now;
    if(t1 < t0 + 0.1 * e2)
      t1 = t0 + 0.1 * e2;
  }

  // Roll the cycle: emits on_beat(beat_time, index) for every beat time
  // crossed. t1 is strictly increasing through here.
  template <typename F>
  void advance(double now, F&& on_beat)
  {
    if(!inited)
      return;
    int guard = 0;
    while(now >= t1 && guard++ < 64)
    {
      t0 = t1;
      t1 += e2;
      ++beat_index;
      on_beat(t0, beat_index);
    }
  }

  double phase(double t) const
  {
    if(!inited || t1 <= t0)
      return 0.;
    return std::clamp((t - t0) / (t1 - t0), 0., 1.);
  }

  double tempo() const { return 60. / e2; }
};

//
// [MON] Confidence monitor. IBT's streaming result is the whole argument: a
// tracker with no "I am lost" monitor scored AMLt 32.2% across abrupt
// transitions vs 100% on the same excerpts alone; automatic re-induction took
// it to 97.2%. Parameters: 3 s window, 1 s hop, score-drop threshold 0.03.
//
struct confidence_monitor
{
  double c_contrast{}, c_margin{}, c_dll{};
  double combined{};

  double acc{}, acc_t{};
  double wins[4]{};
  int wins_n{};
  bool reinduce_request{};

  void reset()
  {
    combined = acc = acc_t = 0.;
    wins_n = 0;
    reinduce_request = false;
  }

  void update(double dt, double contrast, double margin, double dll_rms_rel)
  {
    c_contrast = contrast;
    c_margin = margin;
    c_dll = std::clamp(1. - dll_rms_rel / 0.2, 0., 1.);
    combined = std::clamp(
        0.35 * c_margin + 0.35 * c_contrast + 0.3 * c_dll, 0., 1.);

    acc += combined * dt;
    acc_t += dt;
    if(acc_t >= 1.)
    {
      const double mean = acc / acc_t;
      acc = acc_t = 0.;
      for(int i = 0; i < 3; i++)
        wins[i] = wins[i + 1];
      wins[3] = mean;
      if(wins_n < 4)
        wins_n++;
      if(wins_n >= 4)
      {
        const double prev = (wins[0] + wins[1] + wins[2]) / 3.;
        const double cur = (wins[1] + wins[2] + wins[3]) / 3.;
        if(prev - cur > 0.03)
          reinduce_request = true;
      }
    }
  }
};

// Tap tempo, Mixxx semantics: average the last 80 taps, discard the whole
// series on any interval > 2000 ms, round to 1/12 BPM, minimum 30 BPM.
struct tap_tempo
{
  static constexpr int max_taps = 81; // 80 intervals
  std::vector<double> taps;
  double last_tap{-1e18};

  // returns the tapped BPM, or 0 if not enough taps yet
  double tap(double now)
  {
    if(now - last_tap > 2.0)
      taps.clear();
    last_tap = now;
    taps.push_back(now);
    if((int)taps.size() > max_taps)
      taps.erase(taps.begin());
    if(taps.size() < 2)
      return 0.;
    const double mean = (taps.back() - taps.front()) / double(taps.size() - 1);
    if(mean <= 0.)
      return 0.;
    double bpm = 60. / mean;
    bpm = std::round(bpm * 12.) / 12.;
    if(bpm < 30.)
      return 0.;
    return bpm;
  }
};

//! Beat number carried by an event, if any.
//!
//! int and float name the beat; everything else - impulse, bool, string, list,
//! vec - is a bare beat carrying only its time. bool is deliberately excluded:
//! a toggle sending true/false would otherwise read as beats 1 and 0 forever.
inline std::optional<int> beat_number_of(const ossia::value& v) noexcept
{
  if(auto* i = v.target<int32_t>())
    return *i;
  if(auto* f = v.target<float>())
    return (int)std::llround(*f);
  return std::nullopt;
}

//
// [EV] Tempo and phase from discrete events - one OSC message per beat, a
// footswitch, a clock pulse. The audio path has to infer beat times from a
// noisy onset function; here they are handed to us, so the only real problems
// left are a dropped or doubled message and the mapping from event rate to
// beat rate.
//
struct event_estimator
{
  // Median, not mean: a dropped message doubles one interval and a duplicated
  // one halves it. A mean smears that across the whole estimate; a median of
  // five rejects up to two such outliers outright.
  ossia::median_filter<double, 5> ioi;

  double last_event{-1.};
  double period{0.};     // seconds per BEAT, already divided by events_per_beat
  double confidence{0.};
  int count{};

  void reset()
  {
    ioi.reset();
    last_event = -1.;
    period = 0.;
    confidence = 0.;
    count = 0;
  }

  //! @param t absolute event time, seconds
  //! @param div events per beat (1 = one message per beat, 24 = MIDI clock)
  //! @return true once a usable period estimate exists
  bool push(double t, int div, double min_period, double max_period)
  {
    if(last_event < 0.)
    {
      last_event = t;
      return false;
    }

    const double raw = t - last_event;
    last_event = t;
    if(raw <= 1e-4)
      return false; // two messages in the same sample: not an interval

    double beat_ioi = raw * std::max(1, div);

    // Fold octaves into the allowed range: a dropped message reads as a
    // double-length interval, an extra one as half. Only powers of two are
    // folded - anything else is left for the median to reject, because
    // silently rescaling an arbitrary interval would invent a tempo.
    for(int i = 0; i < 4 && beat_ioi > max_period; i++)
      beat_ioi *= 0.5;
    for(int i = 0; i < 4 && beat_ioi < min_period; i++)
      beat_ioi *= 2.;
    if(beat_ioi < min_period || beat_ioi > max_period)
      return false;

    const double m = ioi(beat_ioi);
    ++count;
    if(count < 2)
      return false;

    period = m;

    // Confidence is the agreement between this interval and the median: a
    // metronome converges to 1, a human tapping settles around 0.6-0.8, a
    // stream with dropouts stays low and the monitor holds instead of chasing.
    const double err = m > 0. ? std::abs(beat_ioi - m) / m : 1.;
    confidence = std::clamp(1. - 4. * err, 0., 1.);
    return period > 0.;
  }
};

} // namespace btrk

/**
 * Beat Tracker: listens to an audio input (a mic on a drummer, a click, a DJ
 * feed) and produces a clock: tempo, beat phase, beat/downbeat pulses and a
 * confidence signal, shaped to drive the timeline's tempo, Speed and position.
 */
struct BeatTracker
{
  halp_meta(name, "Beat Tracker")
  halp_meta(author, "ossia team")
  halp_meta(c_name, "avnd_beat_tracker")
  halp_meta(category, "Timing/Audio")
  halp_meta(description,
      "Audio beat tracker / tempo follower: onset detection, tempo "
      "estimation and a phase-locked clock with confidence monitoring.")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/beat-tracker.html")
  halp_meta(uuid, "8c8c1855-b96f-4231-b40a-453468e7f9ec")

  using band_mode = btrk::spectral_flux_odf::band_mode;
  enum class clock_filter
  {
    dll_2nd_order,
    dll_3rd_order
  };

  //! Where beat evidence comes from. In Events mode the onset detector and the
  //! whole audio chain are bypassed: the estimator and the DLL do not care
  //! whether a beat time came from spectral flux or from an OSC message.
  enum class source_mode
  {
    audio,
    events,
    audio_and_events
  };

  struct ins
  {
    struct : halp::dynamic_audio_bus<"In", double>
    {
      halp_meta(
          description,
          "Audio to track: a microphone on a drummer, a click, a DJ feed, a "
          "bus from elsewhere in the score. Channels are summed to mono. "
          "Ignored when Source is Events.")
    } audio;

    struct : halp::combobox_t<"Source", source_mode>
    {
      halp_meta(
          description,
          "Where beats come from. Audio: listen to the input and detect onsets. "
          "Events: ignore the audio entirely and take beats from the Beat inlet "
          "(OSC, a footswitch, a clock). Audio + Events: both, whichever is "
          "alive.")
      struct range
      {
        std::string_view values[3]{"Audio", "Events", "Audio + Events"};
        source_mode init{source_mode::audio};
      };
    } source;

    // One message per event: an OSC bang, a footswitch, a clock pulse.
    //
    // The payload type selects the meaning, so one cable covers both cases:
    //   int / float -> the value IS the beat number. The tracker then knows
    //                  absolute position, so the downbeat lands on the right
    //                  beat of the bar rather than wherever it started.
    //   anything else (impulse, bool, string, list...) -> a bare beat: phase
    //                  and tempo only.
    //
    // Timestamps are sample-accurate within the tick - quantising them to the
    // block would add up to a whole buffer of jitter per beat, which at 512
    // samples / 44.1 kHz is 11.6 ms, comparable to the entire onset detector's
    // latency.
    struct : halp::accurate<halp::val_port<"Beat", ossia::value>>
    {
      halp_meta(
          description,
          "One message per beat, used when Source is Events. Send an int or a "
          "float and the value is taken as the beat NUMBER, which also gives "
          "the tracker the position in the bar; send anything else (a bang, a "
          "string, a list) and it counts as a plain beat, giving tempo and "
          "phase only. Timestamps are sample-accurate within the buffer.")
    } beat_in;

    struct : halp::spinbox_i32<"Events per beat", halp::irange{1, 24, 1}>
    {
      halp_meta(
          description,
          "How many messages arrive per beat. 1 for one message per beat, 4 "
          "for sixteenth notes in 4/4, 24 for MIDI clock. Only whole beats set "
          "the phase; subdivisions only refine the tempo.")
    } events_per_beat;

    struct : halp::combobox_t<"Band", band_mode>
    {
      halp_meta(
          description,
          "Which part of the spectrum to listen to. Full works on a finished "
          "mix; Kick (30-120 Hz) and Snare (150-400 Hz) are for a miked drum "
          "and ignore everything else; Transient (2-5 kHz) suits clicks and "
          "sticks. Narrowing the band is the single most effective way to stop "
          "the tracker hearing things that are not the beat.")
      struct range
      {
        std::string_view values[5]{"Full", "Kick", "Snare", "Transient", "Kick+Snare"};
        band_mode init{band_mode::full};
      };
    } band;

    // BPM range instead of the traditional hard-coded octave; the toggle is
    // the "None" escape - guessing without an off switch is worse than not
    // guessing.
    struct : halp::toggle<"Limit BPM range", halp::toggle_setup{.init = true}>
    {
      halp_meta(
          description,
          "Restrict the search to Min/Max BPM. This is the main defence "
          "against half- and double-time errors. Turn it off if you would "
          "rather the tracker never second-guessed you.")
    } limit_range;
    struct : halp::spinbox_f32<"Min BPM", halp::range{30., 300., btrk::default_min_bpm}>
    {
      halp_meta(
          description,
          "Slowest tempo considered. Set the pair to bracket the music you "
          "expect: a narrow range locks faster and is far less likely to "
          "settle on half or double the real tempo.")
    } min_bpm;
    struct : halp::spinbox_f32<"Max BPM", halp::range{30., 300., btrk::default_max_bpm}>
    {
      halp_meta(description, "Fastest tempo considered. See Min BPM.")
    } max_bpm;

    // In a sequencer the tempo is free; a seeded tracker beats a cold one on
    // every continuity measure.
    struct : halp::toggle<"Transport tempo hint", halp::toggle_setup{.init = true}>
    {
      halp_meta(
          description,
          "Start from the score's own tempo instead of from nothing. A tracker "
          "given a starting tempo locks in well under a second where a cold "
          "one needs several. Leave this on unless you are deliberately "
          "testing cold-start behaviour.")
    } hint;

    struct : halp::combobox_t<"Clock filter", clock_filter>
    {
      halp_meta(
          description,
          "The loop that turns detected beats into a smooth clock. 2nd order "
          "is the tested default. 3rd order can settle a little flatter on "
          "very steady material but has not been measured against it here.")
      struct range
      {
        std::string_view values[2]{"DLL (2nd order)", "DLL (3rd order)"};
        clock_filter init{clock_filter::dll_2nd_order};
      };
    } filter;

    struct : halp::knob_f32<"Lookahead (ms)", halp::range{0., 250., 0.}>
    {
      halp_meta(
          description,
          "Emit each beat this early, so downstream processing and display "
          "latency land on time. Around 100 ms is nearly free in accuracy "
          "terms. Leave at 0 when driving from Events, which already arrive "
          "on time.")
    } lookahead;
    struct : halp::knob_f32<"Offset (ms)", halp::range{-250., 250., 0.}>
    {
      halp_meta(
          description,
          "Fixed timing trim, applied after Lookahead. Positive delays the "
          "beat, negative advances it. Use it to compensate a mic distance or "
          "a converter's latency.")
    } offset;
    struct : halp::knob_f32<"Gate (dB)", halp::range{-90., 0., -60.}>
    {
      halp_meta(
          description,
          "Input level below which the signal counts as silence, at which "
          "point the clock holds its last tempo rather than chasing noise. "
          "Raise it if room tone or bleed is keeping the tracker awake.")
    } gate;
    struct : halp::toggle<"Whitening", halp::toggle_setup{.init = false}>
    {
      halp_meta(
          description,
          "Continuously equalise the spectrum before onset detection, so quiet "
          "and loud passages contribute alike. Helps on a full mix or a room "
          "mic with a wide dynamic range; unnecessary on a close-miked drum.")
    } whitening;
    struct : halp::spinbox_i32<"Beats per bar", halp::irange{1, 16, 4}>
    {
      halp_meta(
          description,
          "Time signature numerator, used to decide which beats are downbeats "
          "and to compute Bar phase. It does not affect tempo tracking.")
    } beats_per_bar;

    // Manual rescue controls
    struct : halp::impulse_button<"Tap">
    {
      halp_meta(description, "When unlocked, tap in time to set the tempo (taps are averaged; pause for two seconds to start a fresh series). Once locked, a single tap resets the phase so the next beat is now.")
      void update(BeatTracker& self) { self.on_tap(); }
    } tap;
    struct : halp::impulse_button<"Resync">
    {
      halp_meta(description, "Declare that now is beat one. Snaps the downbeat without touching the tempo.")
      void update(BeatTracker& self) { self.on_resync(); }
    } resync;
    struct : halp::impulse_button<"Nudge -">
    {
      halp_meta(description, "Momentarily slow down, to pull the clock back in line with a performer who is ahead. Returns to the tracked tempo on release.")
      void update(BeatTracker& self) { self.on_nudge(-1); }
    } nudge_minus;
    struct : halp::impulse_button<"Nudge +">
    {
      halp_meta(description, "Momentarily speed up, to push the clock forward towards a performer who is behind. Returns to the tracked tempo on release.")
      void update(BeatTracker& self) { self.on_nudge(+1); }
    } nudge_plus;
    struct : halp::impulse_button<"x2">
    {
      halp_meta(description, "Double the tempo without re-detecting, for when the tracker settled an octave low.")
      void update(BeatTracker& self) { self.on_octave(0.5); }
    } dbl;
    struct : halp::impulse_button<"/2">
    {
      halp_meta(description, "Halve the tempo without re-detecting, for when the tracker settled an octave high.")
      void update(BeatTracker& self) { self.on_octave(2.); }
    } hlv;
    struct : halp::toggle<"Hold", halp::toggle_setup{.init = false}>
    {
      halp_meta(
          description,
          "Freeze the tempo at its current value while still following the "
          "beat's phase. For a passage where you trust the tempo but not what "
          "the tracker is hearing - a breakdown, a solo, heavy bleed.")
    } hold;
    struct : halp::toggle<"Follow", halp::toggle_setup{.init = true}>
    {
      halp_meta(
          description,
          "Master switch. Off freezes the clock entirely and stops analysing, "
          "leaving the last tempo and phase in place.")
    } follow;
  } inputs;

  struct
  {
    struct : halp::val_port<"Tempo", double>
    {
      using halp::val_port<"Tempo", double>::operator=;
      halp_meta(
          description,
          "Detected tempo in BPM, smoothed for display. Cable this to an "
          "interval's Tempo inlet to drive the timeline.")
    } tempo{120.};
    struct : halp::val_port<"Speed", double>
    {
      using halp::val_port<"Speed", double>::operator=;
      halp_meta(
          description,
          "Playback rate multiplier: 1 means the score's tempo already "
          "matches, above 1 means speed up to catch the performer. This is the "
          "port to feed a synchroniser or an interval's Speed inlet; unlike "
          "Tempo it corrects phase as well, gradually and within safe limits.")
    } speed{1.};
    struct : halp::val_port<"Phase", double>
    {
      using halp::val_port<"Phase", double>::operator=;
      halp_meta(
          description,
          "Position within the current beat, 0 at the beat and approaching 1 "
          "just before the next. Continuous, so it is safe to map to a "
          "parameter directly.")
    } phase{0.};
    struct : halp::val_port<"Bar phase", double>
    {
      using halp::val_port<"Bar phase", double>::operator=;
      halp_meta(
          description,
          "Position within the current bar, 0 at the downbeat approaching 1. "
          "Depends on Beats per bar and on the downbeat being correct - use "
          "Resync, or send beat numbers, to place it.")
    } bar_phase{0.};
    struct : halp::val_port<"Beat index", int>
    {
      using halp::val_port<"Beat index", int>::operator=;
      halp_meta(
          description,
          "Beats counted since tracking started. Increments by one on every "
          "beat and never goes backwards.")
    } beat_index{0};
    struct : halp::val_port<"Next beat (s)", double>
    {
      using halp::val_port<"Next beat (s)", double>::operator=;
      halp_meta(
          description,
          "Seconds until the next beat, already advanced by Lookahead. Use "
          "this to schedule something ahead of time instead of reacting to "
          "the Beat pulse.")
    } next_beat{0.};
    struct : halp::val_port<"Confidence", double>
    {
      using halp::val_port<"Confidence", double>::operator=;
      halp_meta(
          description,
          "How much the tracker trusts its own reading, 0 to 1. Falls during "
          "breakdowns, silence and material with no clear pulse. Worth mapping "
          "to a fallback so the score can react to the tracker losing the "
          "plot instead of following it blindly.")
    } confidence{0.};
    struct : halp::val_port<"Locked", bool>
    {
      using halp::val_port<"Locked", bool>::operator=;
      halp_meta(
          description,
          "True once the clock has settled: either the evidence is strong or "
          "the tempo has simply held steady for a few seconds. While locked "
          "the tracker corrects gently; while unlocked it hunts.")
    } locked{false};
    struct : halp::val_port<"Valid", bool>
    {
      using halp::val_port<"Valid", bool>::operator=;
      halp_meta(
          description,
          "False when there is nothing to track - the input is below the gate, "
          "or no events have arrived. The clock keeps running on its last "
          "tempo; this tells you not to trust it.")
    } valid{false};

    struct : halp::timed_callback<"Beat">
    {
      halp_meta(
          description,
          "Fires on every beat, timed to the sample. Advanced by Lookahead and "
          "trimmed by Offset.")
    } beat;
    struct : halp::timed_callback<"Downbeat">
    {
      halp_meta(
          description,
          "Fires on the first beat of each bar, according to Beats per bar.")
    } downbeat;
    struct : halp::timed_callback<"Onset">
    {
      halp_meta(
          description,
          "Fires on every detected attack, not only on beats. Useful for "
          "triggering off the raw playing rather than the inferred grid.")
    } onset;
  } outputs;

  // Grouped by the question the user is answering, not by the order the
  // signal flows: "what am I listening to", "what tempo do I expect", "how
  // should the clock behave", "what do I reach for mid-performance".
  //
  // The audio bus and the Beat inlet are not here: they are cables, not
  // controls, and have no widget to place.
  struct ui
  {
    halp_meta(name, "Beat Tracker")
    halp_meta(layout, halp::layouts::tabs)
    halp_meta(background, halp::colors::background_mid)

    struct
    {
      halp_meta(name, "Source")
      halp_meta(layout, halp::layouts::hbox)

      struct
      {
        halp_meta(name, "Input")
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::source> source;
        halp::item<&ins::events_per_beat> events_per_beat;
      } input;

      halp::spacing sp1{.width = 12, .height = 1};

      struct
      {
        halp_meta(name, "Listening")
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::band> band;
        halp::item<&ins::whitening> whitening;
        halp::item<&ins::gate> gate;
      } listening;
    } source_tab;

    struct
    {
      halp_meta(name, "Tempo")
      halp_meta(layout, halp::layouts::hbox)

      struct
      {
        halp_meta(name, "Range")
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::limit_range> limit_range;
        halp::item<&ins::min_bpm> min_bpm;
        halp::item<&ins::max_bpm> max_bpm;
      } range;

      halp::spacing sp1{.width = 12, .height = 1};

      struct
      {
        halp_meta(name, "Musical")
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::hint> hint;
        halp::item<&ins::beats_per_bar> beats_per_bar;
      } musical;
    } tempo_tab;

    struct
    {
      halp_meta(name, "Clock")
      halp_meta(layout, halp::layouts::hbox)

      struct
      {
        halp_meta(name, "Filter")
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::filter> filter;
      } filt;

      halp::spacing sp1{.width = 12, .height = 1};

      struct
      {
        halp_meta(name, "Timing")
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::lookahead> lookahead;
        halp::item<&ins::offset> offset;
      } timing;
    } clock_tab;

    struct
    {
      halp_meta(name, "Performance")
      halp_meta(layout, halp::layouts::hbox)

      struct
      {
        halp_meta(name, "Sync")
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::tap> tap;
        halp::item<&ins::resync> resync;
      } sync;

      halp::spacing sp1{.width = 12, .height = 1};

      struct
      {
        halp_meta(name, "Nudge")
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::nudge_minus> nudge_minus;
        halp::item<&ins::nudge_plus> nudge_plus;
      } nudge;

      halp::spacing sp2{.width = 12, .height = 1};

      struct
      {
        halp_meta(name, "Octave")
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::dbl> dbl;
        halp::item<&ins::hlv> hlv;
      } octave;

      halp::spacing sp3{.width = 12, .height = 1};

      struct
      {
        halp_meta(name, "Engage")
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::hold> hold;
        halp::item<&ins::follow> follow;
      } engage;
    } performance_tab;
  };

  // --- engine state ---
  btrk::spectral_flux_odf m_odf;
  btrk::tempo_estimator m_est;
  btrk::cumulative_tracker m_ct;
  btrk::beat_dll m_dll;
  btrk::confidence_monitor m_mon;
  btrk::tap_tempo m_tap;

  ossia::moving_average_filter<double, 16> m_detrend; // ODF moving mean
  ossia::one_pole_filter<double> m_tempo_display;     // display smoothing

  std::vector<float> m_mono;
  double m_rate{};
  int64_t m_samples{}; // absolute sample clock, matches m_odf.total_samples
  double m_now{};      // seconds, end of last block

  // onset picking
  double m_odf_hist[3]{};
  int m_odf_hist_n{};
  int64_t m_last_onset_frame{-1000};

  // gate
  double m_silence_time{1e9};
  bool m_gate_open{false};

  // event source
  btrk::event_estimator m_ev;
  double m_last_event_time{-1e9};
  int m_cfg_source{-1};

  // bandwidth ladder / startup
  double m_elevated_until{-1.};
  int m_locked_count{};
  bool m_is_locked{};

  // Tempo stability, for the lock criterion. Absolute ODF contrast is
  // material-dependent: a synthetic click reaches 0.75 easily while a real
  // mix peaks around 0.45-0.65, so an absolute threshold means "locked" never
  // fires on actual music - and since lock drives the bandwidth ladder, the
  // loop then never settles to 0.05 Hz and drifts late in a piece. A clock
  // that has held the same period for several seconds IS locked, whatever the
  // onset function's contrast looks like. This mirrors IBT's monitor, which
  // detects a *drop* rather than testing an absolute level.
  static constexpr int tempo_hist_n = 64;
  double m_tempo_hist[tempo_hist_n]{};
  int m_tempo_hist_pos{}, m_tempo_hist_len{};
  double m_tempo_accum_t{};
  double m_tempo_rel_sd{1.};

  // transport hint / feedback-loop break
  bool m_seeded{};
  double m_prev_transport_tempo{-1.};
  double m_last_emitted_tempo{-1.};

  // downbeat bookkeeping
  int64_t m_downbeat_origin{}; // beat index that is "the one"
  int64_t m_last_emitted_beat{-1};

  // output shaping (Mixxx constants)
  double m_speed_trim{};
  static constexpr double sync_adjustment_cap = 0.05; // max +-5% rate correction
  static constexpr double sync_delta_cap = 0.02;      // max change per callback
  static constexpr double sync_error_deadband = 0.01; // beats
  static constexpr double sync_p_gain = 0.7;

  // cached control state, to detect changes cheaply
  float m_cfg_min_bpm{-1.f}, m_cfg_max_bpm{-1.f};
  bool m_cfg_limit{true};
  int m_cfg_band{-1};

  halp::setup setup;

  void prepare(halp::setup s)
  {
    // prepare() is re-invoked at runtime when the buffer grows or the channel
    // count changes: only reset on an actual sample rate change.
    if(s.rate == m_rate && m_rate > 0)
    {
      setup = s;
      return;
    }
    setup = s;
    m_rate = s.rate;
    if(m_rate <= 0)
      return;

    m_odf.whitening = inputs.whitening;
    m_odf.configure(m_rate);
    apply_bpm_range();
    m_ct.beta = m_est.period > 0 ? m_est.period : 60. / 120. * m_odf.fps;
    m_ct.configure();
    m_ct.set_period(m_ct.beta);
    m_dll.reset();
    m_dll.e2 = 0.5;
    m_mon.reset();
    m_detrend.reset();
    m_tempo_display.reset();
    m_mono.resize(4 * setup.frames + 16);
    m_samples = 0;
    m_now = 0;
    m_odf_hist_n = 0;
    m_last_onset_frame = -1000;
    m_silence_time = 1e9;
    m_gate_open = false;
    m_elevated_until = -1.;
    m_locked_count = 0;
    m_tempo_hist_len = 0;
    m_tempo_hist_pos = 0;
    m_tempo_accum_t = 0.;
    m_tempo_rel_sd = 1.;
    m_is_locked = false;
    m_seeded = false;
    m_prev_transport_tempo = -1.;
    m_speed_trim = 0.;
    m_downbeat_origin = 0;
    m_last_emitted_beat = -1;
  }

  void apply_bpm_range()
  {
    const double lo = inputs.limit_range ? inputs.min_bpm.value : 40.;
    const double hi = inputs.limit_range ? inputs.max_bpm.value : 240.;
    m_est.configure(m_odf.fps, std::min(lo, hi), std::max(lo, hi));
    m_cfg_min_bpm = inputs.min_bpm.value;
    m_cfg_max_bpm = inputs.max_bpm.value;
    m_cfg_limit = inputs.limit_range;
  }

  void reconfigure_if_needed()
  {
    if((int)inputs.band.value != m_cfg_band)
    {
      m_odf.set_band_mode(inputs.band.value);
      m_cfg_band = (int)inputs.band.value;
    }
    if(inputs.min_bpm.value != m_cfg_min_bpm || inputs.max_bpm.value != m_cfg_max_bpm
       || bool(inputs.limit_range) != m_cfg_limit)
    {
      apply_bpm_range();
      m_est.reset_context();
    }
    if((int)inputs.source.value != m_cfg_source)
    {
      // Switching source invalidates the interval history: intervals measured
      // from onsets and from messages are not the same measurement.
      m_ev.reset();
      m_last_event_time = -1e9;
      m_est.reset_context();
      m_elevated_until = m_now + 4.;
      m_cfg_source = (int)inputs.source.value;
    }
    m_odf.whitening = inputs.whitening;
    m_dll.order = inputs.filter.value == clock_filter::dll_3rd_order ? 3 : 2;
  }

  // --- manual rescue controls -------------------------------------------
  void on_tap()
  {
    const double bpm = m_tap.tap(m_now);
    if(m_is_locked)
    {
      // locked: a tap resets the phase to "a beat is now"
      if(m_dll.inited)
      {
        m_dll.t0 = m_now;
        m_dll.t1 = m_now + m_dll.e2;
      }
    }
    else if(bpm > 0.)
    {
      // unlocked: taps teach the tempo
      const double period = 60. / bpm;
      m_dll.seed(m_now + period, period);
      seed_estimator_from_period(period);
      m_elevated_until = m_now + 4.;
    }
  }

  void on_resync()
  {
    // snap the downbeat to the one: the next beat becomes beat 0 of a bar,
    // and phase restarts now
    if(m_dll.inited)
    {
      m_dll.t0 = m_now;
      m_dll.t1 = m_now + m_dll.e2;
      m_downbeat_origin = m_dll.beat_index + 1;
    }
  }

  void on_nudge(int direction)
  {
    if(m_dll.inited)
    {
      // 2% of a period per press, spread naturally by the clock
      const double d = direction * 0.02 * m_dll.e2;
      m_dll.t0 += d;
      m_dll.t1 += d;
    }
  }

  void on_octave(double factor)
  {
    // flip the octave without re-detecting
    if(m_dll.inited)
    {
      const double p = std::clamp(
          m_dll.e2 * factor, btrk::beat_dll::min_period, btrk::beat_dll::max_period);
      m_dll.e2 = p;
      m_dll.t1 = std::min(m_dll.t1, m_dll.t0 + p);
      seed_estimator_from_period(p);
    }
  }

  void seed_estimator_from_period(double period_seconds)
  {
    const double frames = period_seconds * m_odf.fps;
    m_est.seed(frames);
    m_ct.set_period(std::clamp<double>(frames, m_est.min_lag, m_est.max_lag));
  }

  // ----------------------------------------------------------------------
  using tick = halp::tick_flicks;
  void operator()(halp::tick_flicks tk)
  {
    if(setup.rate <= 0 || tk.frames <= 0)
      return;
    const double rate = setup.rate;
    const int frames = tk.frames;
    const double block_start = m_samples / rate;
    const double block_dt = frames / rate;

    reconfigure_if_needed();
    handle_transport_hint(tk);

    const auto mode = inputs.source.value;
    const bool use_audio = mode != source_mode::events;
    const bool use_events = mode != source_mode::audio;

    // Events are consumed first so that the gate below can see this block's
    // messages: in Events mode there may be no audio at all, and an audio-RMS
    // gate would hold the loop forever.
    if(use_events)
      process_events(rate, block_start, frames);

    if(use_audio)
    {
      // mono mix + block RMS for the gate
      if((int)m_mono.size() < frames)
        m_mono.resize(frames);
      const int chans = inputs.audio.channels;
      double rms = 0.;
      for(int i = 0; i < frames; i++)
      {
        float acc = 0.f;
        for(int c = 0; c < chans; c++)
          acc += float(inputs.audio.samples[c][i]);
        if(chans > 1)
          acc /= float(chans);
        m_mono[i] = acc;
        rms += acc * acc;
      }
      rms = std::sqrt(rms / std::max(1, frames));
      const double level_db = 20. * std::log10(rms + 1e-12);
      if(level_db > inputs.gate)
      {
        m_silence_time = 0.;
        m_gate_open = true;
      }
      else
      {
        m_silence_time += block_dt;
        // The hang time must exceed the longest inter-beat gap (40 BPM = 1.5 s),
        // otherwise the gate would drop out between beats of slow material.
        if(m_silence_time > 2.0)
          m_gate_open = false; // silence: valid = false, not confidence = 0
      }
    }

    // A live event source opens the gate on its own; in Events mode it is the
    // only thing that can.
    const bool events_alive = use_events && (m_now - m_last_event_time) < event_hang();
    if(!use_audio)
      m_gate_open = events_alive;
    else if(events_alive)
      m_gate_open = true;

    // bandwidth ladder: locked 0.05 / tracking 0.2 / re-inducing 1.0 / hold 0
    const bool holding = inputs.hold || !inputs.follow || !m_gate_open;
    if(holding)
      m_dll.bw = 0.;
    else if(m_now < m_elevated_until)
      m_dll.bw = 1.0;
    else if(m_mon.combined > 0.6)
      m_dll.bw = 0.05;
    else if(m_mon.combined > 0.3)
      m_dll.bw = 0.2;
    else
      m_dll.bw = 1.0;

    // run the analysis chain on the fixed internal hop
    if(use_audio)
      m_odf.process(m_mono.data(), frames, [&](double odf_v, int64_t end_sample) {
        on_odf_frame(odf_v, end_sample, rate, block_start, frames);
      });
    m_samples += frames;
    m_now = m_samples / rate;

    // re-induction: reset the estimator context and open the loop, keeping
    // the previous rate estimate for fast re-locking
    if(m_mon.reinduce_request && !holding)
    {
      m_mon.reinduce_request = false;
      m_est.reset_context();
      m_elevated_until = m_now + 4.;
    }

    emit_beats(rate, block_start, frames);
    update_outputs(tk, block_dt);
  }

  void handle_transport_hint(const halp::tick_flicks& tk)
  {
    if(!inputs.hint || tk.tempo <= 0.)
      return;
    if(!m_seeded)
    {
      // free seed: tempo and downbeat are known in a sequencer
      m_seeded = true;
      m_prev_transport_tempo = tk.tempo;
      const double period = 60. / tk.tempo;
      m_dll.seed(m_now + period, period);
      seed_estimator_from_period(period);
      m_elevated_until = m_now + 4.;
      return;
    }
    if(std::abs(tk.tempo - m_prev_transport_tempo) > 1e-9)
    {
      // The transport tempo changed. If it changed to (about) the tempo we
      // last emitted, it is our own output looping back through a cable:
      // reacting to it would close a feedback loop. Only re-seed on genuinely
      // external changes.
      if(m_last_emitted_tempo < 0.
         || std::abs(tk.tempo - m_last_emitted_tempo) > 0.5)
      {
        seed_estimator_from_period(60. / tk.tempo);
        if(!m_is_locked)
          m_dll.seed(m_now + 60. / tk.tempo, 60. / tk.tempo, false);
        m_elevated_until = m_now + 4.;
      }
      m_prev_transport_tempo = tk.tempo;
    }
  }

  void on_odf_frame(
      double odf_v, int64_t end_sample, double rate, double block_start, int frames)
  {
    // detrend: subtract the moving mean, half-wave rectify
    const double mean = m_detrend(odf_v);
    const double d = std::max(0., odf_v - mean);

    // onset picking: local maximum above an adaptive threshold
    m_odf_hist[0] = m_odf_hist[1];
    m_odf_hist[1] = m_odf_hist[2];
    m_odf_hist[2] = odf_v;
    if(m_odf_hist_n < 3)
      m_odf_hist_n++;
    const int64_t cur_frame = m_odf.frame_count;
    if(m_odf_hist_n >= 3 && m_gate_open && m_odf_hist[1] > m_odf_hist[2]
       && m_odf_hist[1] >= m_odf_hist[0] && m_odf_hist[1] > 1.5 * mean + 1e-4
       && cur_frame - m_last_onset_frame > (int64_t)(0.03 * m_odf.fps))
    {
      m_last_onset_frame = cur_frame;
      const int64_t onset_sample = end_sample - m_odf.hop;
      const int64_t off = onset_sample - (int64_t)(block_start * rate);
      outputs.onset(std::clamp<int64_t>(off, 0, frames - 1));
    }

    // The estimator and the cumulative score always consume the frame, gated
    // or not: their timelines must stay contiguous. Skipping frames while a
    // gate is closed compresses the apparent beat period by exactly the
    // skipped time (a 100 BPM train came out as 114.9 BPM that way). Silence
    // contributes zero flux, which is the correct evidence for it. The gate
    // only drives validity and the DLL hold.
    if(!inputs.follow)
      return;

    // tempo estimation
    if(m_est.push(d) && m_est.period > 0.)
    {
      m_ct.set_period(m_est.period);
      // While not yet locked, adopt the estimator's period directly: the
      // DLL's slow, capped corrections are for tracking, not acquisition -
      // converging from the default period to a distant one at 5% per beat
      // takes tens of seconds.
      if(!m_is_locked && m_est.confidence > 0.2 && m_dll.inited && m_dll.bw > 0.)
      {
        m_dll.e2 = std::clamp(
            m_est.period / m_odf.fps, btrk::beat_dll::min_period,
            btrk::beat_dll::max_period);
      }
    }

    // cumulative score + beat prediction -> DLL observation
    const int64_t predicted_frame = m_ct.push(d);
    if(predicted_frame >= 0 && !inputs.hold)
    {
      // ODF latency: the flux at frame n reflects audio a few hops earlier
      // ((3+mu) * hop); compensate the constant part here, the Offset control
      // trims the rest.
      const double tb = (predicted_frame - 3) * m_odf.hop / rate;
      m_dll.observe(tb, (double)end_sample / rate);
    }
  }

  //! The user BPM range as beat periods, matching what reconfigure_if_needed
  //! hands the audio estimator. "Limit BPM range" off widens to 40-240 rather
  //! than removing the bound entirely: without any bound a single spurious
  //! interval can pull the estimate anywhere.
  std::pair<double, double> period_bounds() const
  {
    const double lo = inputs.limit_range ? inputs.min_bpm.value : 40.;
    const double hi = inputs.limit_range ? inputs.max_bpm.value : 240.;
    return {60. / std::max(1., hi), 60. / std::max(1., lo)};
  }

  //! How long an event source may stay silent before it counts as lost.
  //! Must exceed the longest plausible gap between messages, otherwise a slow
  //! event rate would drop out between its own beats.
  double event_hang() const
  {
    const double p = m_ev.period > 0. ? m_ev.period : m_dll.e2;
    return std::max(2.0, 4. * p);
  }

  //! Drive the clock straight from discrete events. Each trigger is a beat
  //! (or a subdivision of one); the DLL is told the beat time and folds the
  //! phase error itself, exactly as it does for an audio-derived beat.
  void process_events(double rate, double block_start, int frames)
  {
    const int div = std::max(1, (int)inputs.events_per_beat);
    const double block_end = block_start + frames / rate;
    const auto [min_p, max_p] = period_bounds();

    for(auto& [off, val] : inputs.beat_in.values)
    {
      const int64_t o = std::clamp<int64_t>(off, 0, frames > 0 ? frames - 1 : 0);
      const double t = block_start + double(o) / rate;

      const std::optional<int> pending_number = btrk::beat_number_of(val);

      m_last_event_time = t;
      outputs.onset(o);

      if(!inputs.follow || inputs.hold)
        continue;

      if(!m_ev.push(t, div, min_p, max_p))
        continue;

      // Only whole beats carry phase: a subdivision between beats would drag
      // the clock onto the subdivision grid.
      const bool on_beat = (div == 1) || (pending_number && (*pending_number % div == 0));
      if(!on_beat)
        continue;

      if(!m_dll.inited)
      {
        m_dll.seed(t + m_ev.period, m_ev.period);
        m_elevated_until = m_now + 4.;
      }
      else
      {
        // Acquisition: adopt the measured period outright rather than letting
        // the loop crawl to it at 5% per beat, same reasoning as the audio path.
        if(!m_is_locked && m_ev.confidence > 0.2 && m_dll.bw > 0.)
          m_dll.e2 = std::clamp(
              m_ev.period, btrk::beat_dll::min_period, btrk::beat_dll::max_period);

        m_dll.observe(t + m_dll.e2, std::min(t, block_end));
      }

      // With a beat number we know absolute position, so the downbeat can be
      // placed on the right beat of the bar instead of wherever we started.
      if(pending_number)
      {
        const int bpb = std::max(1, (int)inputs.beats_per_bar);
        const int beat_in_bar = ((*pending_number / div) % bpb + bpb) % bpb;
        m_downbeat_origin = m_dll.beat_index + 1 - beat_in_bar;
      }
    }
  }

  void emit_beats(double rate, double block_start, int frames)
  {
    if(!m_dll.inited)
      return;
    const double lookahead = inputs.lookahead * 1e-3;
    const double offset = inputs.offset * 1e-3;
    const double block_end = block_start + frames / rate;

    // Scan the pending beats: beat k (global index beat_index + 1 + k) is due
    // at t1 + k*e2; it is emitted `lookahead + offset` early.
    for(int k = 0; k < 8; k++)
    {
      const double tb = m_dll.t1 + k * m_dll.e2;
      const double emit_time = tb - lookahead - offset;
      if(emit_time >= block_end)
        break;
      const int64_t index = m_dll.beat_index + 1 + k;
      if(index <= m_last_emitted_beat)
        continue;
      if(emit_time < block_start - 0.5 * m_dll.e2)
        continue; // stale
      const int64_t off = std::clamp<int64_t>(
          (int64_t)std::lround((emit_time - block_start) * rate), 0, frames - 1);
      m_last_emitted_beat = index;
      outputs.beat(off);
      const int bpb = std::max(1, (int)inputs.beats_per_bar);
      if(((index - m_downbeat_origin) % bpb + bpb) % bpb == 0)
        outputs.downbeat(off);
    }

    // roll the oscillator state up to now
    m_dll.advance(m_now, [](double, int64_t) {});
  }

  void update_outputs(const halp::tick_flicks& tk, double block_dt)
  {
    const bool valid = m_gate_open && m_dll.inited;

    // confidence. In Events mode the cumulative score and the ACF estimator
    // never ran, so their values are stale; interval agreement is the evidence
    // we actually have. With both sources, take whichever is more sure.
    const double rel_rms
        = m_dll.e2 > 0. ? std::sqrt(m_dll.innovation_rms) / m_dll.e2 : 1.;
    double contrast = m_ct.last_contrast;
    double est_conf = m_est.confidence;
    if(inputs.source.value != source_mode::audio)
    {
      const double ev = (m_now - m_last_event_time) < event_hang() ? m_ev.confidence : 0.;
      if(inputs.source.value == source_mode::events)
      {
        contrast = ev;
        est_conf = ev;
      }
      else
      {
        contrast = std::max(contrast, ev);
        est_conf = std::max(est_conf, ev);
      }
    }
    m_mon.update(block_dt, contrast, est_conf, rel_rms);

    // Tempo stability over the last ~4 s, sampled at 16 Hz.
    m_tempo_accum_t += block_dt;
    if(m_tempo_accum_t >= 1. / 16. && m_dll.inited)
    {
      m_tempo_accum_t = 0.;
      m_tempo_hist[m_tempo_hist_pos] = m_dll.tempo();
      m_tempo_hist_pos = (m_tempo_hist_pos + 1) % tempo_hist_n;
      m_tempo_hist_len = std::min(m_tempo_hist_len + 1, tempo_hist_n);

      if(m_tempo_hist_len >= tempo_hist_n / 2)
      {
        double mean = 0.;
        for(int i = 0; i < m_tempo_hist_len; i++)
          mean += m_tempo_hist[i];
        mean /= m_tempo_hist_len;
        double var = 0.;
        for(int i = 0; i < m_tempo_hist_len; i++)
          var += (m_tempo_hist[i] - mean) * (m_tempo_hist[i] - mean);
        m_tempo_rel_sd
            = mean > 1. ? std::sqrt(var / m_tempo_hist_len) / mean : 1.;
      }
    }

    // Locked if the evidence is strong OR the clock has simply been steady:
    // holding a period to within 0.5% for four seconds, with small DLL
    // innovations, is a lock by any useful definition.
    const bool steady = m_tempo_rel_sd < 0.005 && rel_rms < 0.15
                        && m_tempo_hist_len >= tempo_hist_n / 2;
    if((m_mon.combined > 0.6 || steady) && valid)
      m_locked_count = std::min(m_locked_count + 1, 1000);
    else
      m_locked_count = std::max(m_locked_count - 2, 0);
    m_is_locked = m_locked_count > 20;

    const double raw_tempo = m_dll.inited ? m_dll.tempo() : 120.;
    const double alpha = ossia::lag_alpha(0.25, block_dt);
    const double display_tempo = m_tempo_display(raw_tempo, alpha);

    // Rate output, Mixxx-shaped: emit a rate, not a position; correct phase
    // through the rate, capped and slew-limited, spread over the next beats.
    const double ref_tempo = (inputs.hint && tk.tempo > 0.) ? tk.tempo : 120.;
    const double tempo_ratio = raw_tempo / ref_tempo;
    double err_beats = 0.;
    if(m_dll.inited && valid)
    {
      const double our_phase = m_dll.phase(m_now);
      const double transport_phase
          = tk.end_position_in_quarters - std::floor(tk.end_position_in_quarters);
      err_beats = our_phase - transport_phase;
      err_beats -= std::round(err_beats); // wrap to +-0.5 beat
      if(std::abs(err_beats) < sync_error_deadband)
        err_beats = 0.;
    }
    double trim_target
        = std::clamp(sync_p_gain * err_beats, -sync_adjustment_cap, sync_adjustment_cap);
    m_speed_trim
        += std::clamp(trim_target - m_speed_trim, -sync_delta_cap, sync_delta_cap);

    outputs.tempo = display_tempo;
    m_last_emitted_tempo = display_tempo;
    outputs.speed = valid ? tempo_ratio * (1. + m_speed_trim) : 1.;
    outputs.phase = m_dll.phase(m_now);
    const int bpb = std::max(1, (int)inputs.beats_per_bar);
    const int64_t idx = m_dll.beat_index < 0 ? 0 : m_dll.beat_index;
    const int64_t in_bar = ((idx - m_downbeat_origin) % bpb + bpb) % bpb;
    outputs.bar_phase = (in_bar + outputs.phase) / bpb;
    outputs.beat_index = (int)idx;
    const double lookahead = inputs.lookahead * 1e-3;
    outputs.next_beat
        = m_dll.inited ? std::max(0., (m_dll.t1 - m_now) - lookahead) : 0.;
    outputs.confidence = m_mon.combined;
    outputs.locked = m_is_locked;
    outputs.valid = valid;
  }
};
}
