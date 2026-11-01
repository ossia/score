#pragma once
#include <ossia/audio/fft.hpp>

#include <Yin.h>

#include <QString>

#include <Ui/Monitor/MidiText.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <optional>
#include <span>
#include <vector>

/**
 * Power spectra calibrated in dBFS: a full-scale sine reads 0 dB at its
 * frequency whatever the FFT size and window, and a band sums the energy of
 * its bins so that broadband noise reads its true band level.
 */
namespace Ui::Monitor
{
enum class Window
{
  Hann,
  BlackmanHarris,
  FlatTop
};

struct SpectrumAnalysis
{
  int size{};
  double rate{48000.};
  ossia::fft fft{16};
  std::vector<double> window;
  double coherent_gain{1.};
  double sum_sq{1.};
  Window kind{};
  //! Power of each bin, normalized so that 10 log10 of it is the level in
  //! dBFS of a sine centred on that bin.
  std::vector<double> power;

  void prepare(int n, double sample_rate, Window w)
  {
    n = std::max(n, 64);
    rate = sample_rate;
    if(n == size && w == kind && !window.empty())
      return;
    if(n != size)
    {
      size = n;
      fft.reset(n);
      power.assign(n / 2 + 1, 0.);
    }
    kind = w;
    window.resize(n);
    constexpr double pi = std::numbers::pi;
    for(int i = 0; i < n; i++)
    {
      const double t = double(i) / n;
      switch(w)
      {
        case Window::Hann:
          window[i] = 0.5 - 0.5 * std::cos(2 * pi * t);
          break;
        case Window::BlackmanHarris:
          window[i] = 0.35875 - 0.48829 * std::cos(2 * pi * t)
                      + 0.14128 * std::cos(4 * pi * t) - 0.01168 * std::cos(6 * pi * t);
          break;
        case Window::FlatTop:
          window[i] = 0.21557895 - 0.41663158 * std::cos(2 * pi * t)
                      + 0.277263158 * std::cos(4 * pi * t)
                      - 0.083578947 * std::cos(6 * pi * t)
                      + 0.006947368 * std::cos(8 * pi * t);
          break;
      }
    }
    double s = 0, s2 = 0;
    for(double v : window)
    {
      s += v;
      s2 += v * v;
    }
    coherent_gain = s;
    sum_sq = s2;
  }

  //! Analyses `size` samples; `x` must hold at least that many.
  template <typename T>
  void analyse(const T* x) noexcept
  {
    auto* in = fft.input();
    for(int i = 0; i < size; i++)
      in[i] = x[i] * window[i];
    const auto* out = fft.execute();
    // |X| * 2 / sum(w) is the amplitude of a sine on that bin
    const double k = 4. / (coherent_gain * coherent_gain);
    const int bins = size / 2 + 1;
    for(int b = 0; b < bins; b++)
      power[b] = (out[b][0] * out[b][0] + out[b][1] * out[b][1]) * k;
  }

  double binFrequency(double bin) const noexcept { return bin * rate / size; }
  double frequencyBin(double f) const noexcept { return f * size / rate; }

  //! Equivalent noise bandwidth of the window, in bins: a sine's power
  //! spreads over that many bins of `power`.
  double enbw() const noexcept { return size * sum_sq / (coherent_gain * coherent_gain); }

  //! Energy of the bins of `p` (a spectrum of this analysis, averaged or
  //! not) between two frequencies, as the level a sine of that power would
  //! read: the window's equivalent noise bandwidth is divided out.
  double bandPower(std::span<const double> p, double f0, double f1) const noexcept
  {
    if(p.empty())
      return 0.;
    const int last = int(p.size()) - 1;
    const int b0 = std::clamp(int(std::ceil(frequencyBin(f0))), 0, last);
    const int b1 = std::clamp(int(std::floor(frequencyBin(f1))), 0, last);
    if(b1 < b0)
    {
      // Band narrower than a bin: the share of the bin it falls in that its
      // width covers, so that the level per Hz is the same as for wide bands
      const int b = std::clamp(int(std::lround(frequencyBin((f0 + f1) / 2))), 0, last);
      return p[b] * (frequencyBin(f1) - frequencyBin(f0)) / enbw();
    }
    double sum = 0;
    for(int b = b0; b <= b1; b++)
      sum += p[b];
    return sum / enbw();
  }

  double bandPower(double f0, double f1) const noexcept { return bandPower(power, f0, f1); }
};

//! Centre frequencies of the IEC 61260 base-10 third-octave bands, 20 Hz to
//! 20 kHz, and their edges.
struct ThirdOctave
{
  static constexpr int count = 31;
  static double centre(int i) noexcept { return 1000. * std::pow(10., (i - 17) / 10.); }
  static double lower(int i) noexcept { return centre(i) * std::pow(10., -1. / 20.); }
  static double upper(int i) noexcept { return centre(i) * std::pow(10., 1. / 20.); }
  //! IEC 61260 nominal centre frequencies
  static constexpr std::array<const char*, count> labels{
      "20",  "25",  "31.5", "40",   "50",   "63",  "80",    "100", "125", "160", "200",
      "250", "315", "400",  "500",  "630",  "800", "1k",    "1.25k", "1.6k", "2k", "2.5k",
      "3.15k", "4k", "5k",  "6.3k", "8k",   "10k", "12.5k", "16k", "20k"};
};

//! Fundamental frequency of a signal by YIN (the vendored Gist), or 0 when
//! it has none. The lowest it finds is the rate over half the frame.
class PitchTracker
{
public:
  double operator()(std::span<const float> frame, double rate, double fmax = 2000.)
  {
    if(rate <= 0 || frame.size() < 64)
      return 0.;
    if(!m_yin || m_rate != rate)
    {
      m_yin.emplace(int(rate));
      m_yin->setMaxFrequency(float(fmax));
      m_rate = rate;
    }
    m_frame.assign(frame.begin(), frame.end());
    const float f = m_yin->pitchYin(m_frame);
    return f > 0.f ? double(f) : 0.;
  }

private:
  std::optional<Yin<float>> m_yin;
  std::vector<float> m_frame;
  double m_rate{};
};

//! "A4", "C#3"
inline QString noteName(double f, double* cents = nullptr)
{
  if(f <= 0)
    return {};
  const double midi = 69. + 12. * std::log2(f / 440.);
  const int n = int(std::lround(midi));
  if(cents)
    *cents = (midi - n) * 100.;
  return midiNoteName(n);
}
}
