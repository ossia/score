#pragma once
#include <Process/Monitoring/MonitorLink.hpp>

#include <Ui/Monitor/LayerHelpers.hpp>
#include <Process/Monitoring/SampleRing.hpp>

#include <Effect/EffectLayer.hpp>

#include <QColor>
#include <QElapsedTimer>
#include <QImage>

#include <Ui/Monitor/SpectrumAnalysis.hpp>
#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <array>
#include <cmath>
#include <optional>
#include <vector>

namespace Process
{
class ControlInlet;
}

namespace Ui::Spectrum
{
enum class Mode
{
  Analyser,
  RTA,
  Spectrogram
};
inline constexpr std::string_view mode_names[]{"Analyser", "RTA", "Spectrogram"};

enum class Size
{
  S1024,
  S2048,
  S4096,
  S8192,
  S16384,
  S32768
};
inline constexpr std::string_view size_names[]{"1024", "2048", "4096", "8192", "16384", "32768"};

inline constexpr std::string_view window_names[]{"Hann", "Blackman-Harris", "Flat top"};

enum class Slope
{
  Flat,
  Pink,
  Tilt45
};
inline constexpr std::string_view slope_names[]{"0 dB/oct", "3 dB/oct", "4.5 dB/oct"};

enum class Smoothing
{
  Off,
  Twelfth,
  Sixth,
  Third
};
inline constexpr std::string_view smoothing_names[]{"Off", "1/12 oct", "1/6 oct", "1/3 oct"};

enum class Channels
{
  Sum,
  Each,
  MidSide,
  Left,
  Right
};
inline constexpr std::string_view channels_names[]{"Sum", "Each", "Mid and side", "Left", "Right"};

struct SpectrumMonitor final : Process::Monitor
{
  Process::SampleRing ring{1 << 17};
};

inline double slopeDbPerOctave(Slope s)
{
  return s == Slope::Pink ? 3. : s == Slope::Tilt45 ? 4.5 : 0.;
}

struct Layer;

struct Node
{
  halp_meta(name, "Spectrum")
  halp_meta(c_name, "SpectrumAnalyser")
  halp_meta(category, "Monitoring/Audio")
  halp_meta(author, "ossia score")
  halp_meta(manual_url, "")
  halp_meta(
      description,
      "Spectrum analyser, third-octave RTA and spectrogram, calibrated "
      "so that a full-scale sine reads 0 dBFS")
  halp_meta(uuid, "277e8d4c-afea-490f-bf45-21ac2180871c")
  halp_flag(fully_custom_item);
  using Layer = Ui::Spectrum::Layer;

  struct
  {
    halp::dynamic_audio_bus<"Audio", double> audio;
    struct : halp::combobox_t<"Mode", Mode>
    {
      struct range
      {
        std::string_view values[3]{mode_names[0], mode_names[1], mode_names[2]};
        Mode init{Mode::Analyser};
      };
    } mode;
    struct : halp::combobox_t<"FFT size", Size>
    {
      halp_meta(description, "Larger: finer frequency resolution, slower response")
      struct range
      {
        std::string_view values[6]{
            size_names[0], size_names[1], size_names[2],
            size_names[3], size_names[4], size_names[5]};
        Size init{Size::S8192};
      };
    } size;
    struct : halp::combobox_t<"Window", Ui::Monitor::Window>
    {
      halp_meta(description, "Hann for general use, Blackman-Harris for dynamic range, flat top for exact levels")
      struct range
      {
        std::string_view values[3]{window_names[0], window_names[1], window_names[2]};
        Ui::Monitor::Window init{Ui::Monitor::Window::Hann};
      };
    } window;
    struct : halp::spinbox_f32<"Averaging", halp::range{0., 10., 0.3}>
    {
      halp_meta(description, "Seconds of exponential averaging")
    } averaging;
    struct : halp::combobox_t<"Slope", Slope>
    {
      halp_meta(description, "Tilt around 1 kHz: 3 dB/oct shows pink noise flat")
      struct range
      {
        std::string_view values[3]{slope_names[0], slope_names[1], slope_names[2]};
        Slope init{Slope::Flat};
      };
    } slope;
    struct : halp::combobox_t<"Smoothing", Smoothing>
    {
      struct range
      {
        std::string_view values[4]{
            smoothing_names[0], smoothing_names[1], smoothing_names[2], smoothing_names[3]};
        Smoothing init{Smoothing::Off};
      };
    } smoothing;
    struct : halp::spinbox_f32<"Floor", halp::range{-160., -20., -100.}>
    {
    } floor;
    struct : halp::spinbox_f32<"Ceiling", halp::range{-60., 24., 0.}>
    {
    } ceiling;
    struct : halp::combobox_t<"Channels", Channels>
    {
      struct range
      {
        std::string_view values[5]{
            channels_names[0], channels_names[1], channels_names[2], channels_names[3],
            channels_names[4]};
        Channels init{Channels::Sum};
      };
    } channels;
    halp::toggle<"Peak hold"> peak_hold;
  } inputs;

  struct
  {
    struct : halp::val_port<"Peak", float>
    {
      halp_meta(description, "Frequency of the strongest component, in Hz")
    } peak;
    struct : halp::val_port<"Centroid", float>
    {
      halp_meta(description, "Spectral centroid, in Hz")
    } centroid;
    //! Set only when the outlets are analysed: each message is a new list
    struct : halp::val_port<"Bands", std::optional<std::vector<float>>>
    {
      halp_meta(description, "Levels of the 31 third-octave bands from 20 Hz to 20 kHz, in dBFS")
    } bands;
  } outputs;

  std::shared_ptr<SpectrumMonitor> monitor = std::make_shared<SpectrumMonitor>();
  Ui::Monitor::SpectrumAnalysis analysis;
  std::vector<double> history;
  std::vector<double> ordered;
  int write_pos{};
  double since{};
  double rate{};

  //! Also called on the execution thread when the channel count changes:
  //! the analysis only depends on the rate.
  void prepare(halp::setup s)
  {
    const double r = s.rate > 0 ? s.rate : 48000.;
    if(r == rate)
      return;
    rate = r;
    monitor->ring.set_rate(rate);
    analysis.prepare(4096, rate, Ui::Monitor::Window::Hann);
    history.assign(4096, 0.);
    ordered.assign(4096, 0.);
    write_pos = 0;
  }

  void operator()(int frames)
  {
    const int nch = inputs.audio.channels;
    if(nch <= 0 || frames <= 0 || history.empty())
      return;
    if(monitor->watched())
      monitor->ring.write(nch, frames, [&](int c) { return inputs.audio[c]; });

    // The outlets: a mono mix analysed twenty times a second
    const int n = int(history.size());
    for(int i = 0; i < frames; i++)
    {
      double x = 0;
      for(int c = 0; c < nch; c++)
        x += inputs.audio[c][i];
      history[write_pos] = x / nch;
      write_pos = (write_pos + 1) % n;
    }
    since += frames;
    if(since < rate / 20.)
      return;
    since = 0;
    for(int i = 0; i < n; i++)
      ordered[i] = history[(write_pos + i) % n];
    analysis.analyse(ordered.data());

    const auto& p = analysis.power;
    int best = 1;
    double num = 0, den = 0;
    for(int b = 1; b < int(p.size()); b++)
    {
      if(p[b] > p[best])
        best = b;
      num += p[b] * analysis.binFrequency(b);
      den += p[b];
    }
    outputs.peak.value = float(analysis.binFrequency(best));
    outputs.centroid.value = den > 0 ? float(num / den) : 0.f;
    auto& bands = outputs.bands.value.emplace(Ui::Monitor::ThirdOctave::count);
    for(int i = 0; i < Ui::Monitor::ThirdOctave::count; i++)
      bands[i] = float(10. * std::log10(std::max(
          analysis.bandPower(
              Ui::Monitor::ThirdOctave::lower(i), Ui::Monitor::ThirdOctave::upper(i)),
          1e-20)));
  }
};

struct Layer : public Process::EffectLayerView
{
  static constexpr double fmin = 20.;
  static constexpr double left_margin = 30.;
  static constexpr double bottom_margin = 14.;
  //! Columns of the spectrogram per second of signal
  static constexpr double columns_per_second = 50.;

  struct Curve
  {
    std::vector<double> avg;
    std::vector<double> peak;
    //! The last analysis, unaveraged: what the spectrogram reads
    std::vector<double> now;
    QColor color;
    QString name;
  };

  //! What the curves are drawn with, read from the controls once per paint
  struct Display
  {
    Smoothing smoothing{};
    double slope{};
  };

  Layer(
      const Process::ProcessModel& process, const Process::Context& doc,
      QGraphicsItem* parent);

  void hoverMoveEvent(QGraphicsSceneHoverEvent* ev) override;
  void hoverLeaveEvent(QGraphicsSceneHoverEvent*) override;
  void paint_impl(QPainter* p) const override;
  //! A double click fits Floor and Ceiling to the levels shown
  void mousePressEvent(QGraphicsSceneMouseEvent* ev) override;

private:
  Mode mode() const;
  int fftSize() const;
  double floorDb() const;
  double ceilingDb() const;
  double fmax() const { return std::min(20000., m_rate / 2.); }
  Display display() const;

  void refresh(SpectrumMonitor& mon);
  double levelAt(const Display& d, const std::vector<double>& power, double f0, double f1) const;
  QRectF plotRect() const;
  double xOfFreq(const QRectF& r, double f) const;
  double freqOfX(const QRectF& r, double x) const;
  double yOfDb(const QRectF& r, double db) const;
  void addSonogramColumns(int count);

  void paintGrid(QPainter* p, const QRectF& r) const;
  void paintAnalyser(QPainter* p) const;
  void paintRta(QPainter* p) const;
  void paintSonogram(QPainter* p) const;

  const Process::ControlInlet* m_mode{};
  const Process::ControlInlet* m_size{};
  const Process::ControlInlet* m_window{};
  const Process::ControlInlet* m_avg{};
  const Process::ControlInlet* m_slope{};
  const Process::ControlInlet* m_smooth{};
  const Process::ControlInlet* m_floor{};
  const Process::ControlInlet* m_ceiling{};
  const Process::ControlInlet* m_channels{};
  const Process::ControlInlet* m_hold{};
  Process::MonitorLink* m_link{};
  const score::CommandStackFacade& m_stack;
  Ui::Monitor::DoubleClick m_doubleClick;

  Ui::Monitor::SpectrumAnalysis m_analysis;
  std::vector<std::vector<float>> m_in;
  std::vector<float> m_mix;
  std::vector<Curve> m_curves;
  QElapsedTimer m_clock;

  // Spectrogram: a ring of columns, one per 1 / columns_per_second of signal
  QImage m_sonogram;
  QImage m_colour_bar;
  int m_column{};
  uint64_t m_column_written{};

  double m_rate{48000.};
  std::optional<QPointF> m_hover;
};

}
