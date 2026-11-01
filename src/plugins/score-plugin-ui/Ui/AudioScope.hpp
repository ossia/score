#pragma once
#include <Process/Monitoring/MonitorLink.hpp>
#include <Process/Monitoring/SampleRing.hpp>

#include <Effect/EffectLayer.hpp>

#include <ossia/audio/meters.hpp>

#include <Ui/Monitor/LayerHelpers.hpp>
#include <Ui/Monitor/Paint.hpp>
#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <array>
#include <cmath>
#include <numbers>
#include <vector>

namespace Process
{
class ControlInlet;
}

namespace Ui::AudioScope
{
enum class Mode
{
  Oscilloscope,
  XY,
  Goniometer,
  Correlation
};
inline constexpr std::string_view mode_names[]{
    "Oscilloscope", "XY", "Goniometer", "Correlation"};

enum class Trigger
{
  Free,
  Rising,
  Falling,
  Pitch
};
inline constexpr std::string_view trigger_names[]{"Free", "Rising", "Falling", "Pitch"};

enum class Polar
{
  Lissajous,
  PolarSample,
  PolarLevel
};
inline constexpr std::string_view polar_names[]{"Lissajous", "Polar sample", "Polar level"};

//! Longest oscilloscope window: with the trigger search over two windows,
//! it fits the ring at 192 kHz.
inline constexpr double max_window_ms = 300.;

struct ScopeMonitor final : Process::Monitor
{
  Process::SampleRing ring{1 << 18};
  std::atomic<float> correlation{};
};

/**
 * The trigger point of `x`: the latest index in [0, last] where the signal
 * crosses `level` in the given direction, with a little hysteresis so that
 * noise around the level does not retrigger. -1 when there is none.
 */
inline int findTrigger(const float* x, int last, float level, bool rising) noexcept
{
  const float hyst = 0.01f;
  for(int i = last; i > 0; i--)
  {
    if(rising ? (x[i - 1] < level && x[i] >= level) : (x[i - 1] > level && x[i] <= level))
    {
      // Confirm that the signal was clearly on the other side before
      for(int j = i - 1; j >= std::max(0, i - 64); j--)
        if(rising ? x[j] < level - hyst : x[j] > level + hyst)
          return i;
    }
  }
  return -1;
}

/**
 * The period of `x` in samples, from the median distance between its rising
 * crossings of its mean, located between samples, or 0 when it has no clear
 * period. A crossing counts once the signal has gone a tenth of its swing
 * below the mean, so that noise near the mean does not add crossings.
 */
inline double estimatePeriod(const float* x, int n) noexcept
{
  if(n < 4)
    return 0.;
  double mean = 0;
  for(int i = 0; i < n; i++)
    mean += x[i];
  mean /= n;
  double swing = 0;
  for(int i = 0; i < n; i++)
    swing = std::max(swing, std::abs(x[i] - mean));
  if(swing < 1e-6)
    return 0.;
  const double arm = mean - 0.1 * swing;

  std::array<double, 64> crossings{};
  int count = 0;
  bool armed = false;
  for(int i = 1; i < n && count < int(crossings.size()); i++)
  {
    if(x[i] < arm)
      armed = true;
    const double a = x[i - 1] - mean, b = x[i] - mean;
    if(armed && a < 0. && b >= 0.)
    {
      crossings[count++] = (i - 1) + (-a) / (b - a);
      armed = false;
    }
  }
  if(count < 3)
    return 0.;
  std::array<double, 63> d{};
  for(int i = 1; i < count; i++)
    d[i - 1] = crossings[i] - crossings[i - 1];
  std::nth_element(d.begin(), d.begin() + (count - 1) / 2, d.begin() + (count - 1));
  return d[(count - 1) / 2];
}

struct Layer;

struct Node
{
  halp_meta(name, "Audio scope")
  halp_meta(c_name, "AudioScope")
  halp_meta(category, "Monitoring/Audio")
  halp_meta(author, "ossia score")
  halp_meta(manual_url, "")
  halp_meta(
      description,
      "Oscilloscope with edge and pitch triggers, XY display, goniometer and "
      "phase correlation meter")
  halp_meta(uuid, "57966fbd-574f-48cc-9e26-74397352f3dd")
  halp_flag(fully_custom_item);
  using Layer = Ui::AudioScope::Layer;

  struct
  {
    halp::dynamic_audio_bus<"Audio", double> audio;

    struct : halp::combobox_t<"Mode", Mode>
    {
      struct range
      {
        std::string_view values[4]{
            mode_names[0], mode_names[1], mode_names[2], mode_names[3]};
        Mode init{Mode::Oscilloscope};
      };
    } mode;
    struct : halp::spinbox_f32<"Window", halp::range{0.1, max_window_ms, 20.}>
    {
      halp_meta(description, "Milliseconds shown across the oscilloscope")
    } window;
    struct : halp::combobox_t<"Trigger", Trigger>
    {
      halp_meta(
          description,
          "Free running, on a rising or falling edge of the first channel, or "
          "synchronized to its period, two periods across")
      struct range
      {
        std::string_view values[4]{
            trigger_names[0], trigger_names[1], trigger_names[2], trigger_names[3]};
        Trigger init{Trigger::Rising};
      };
    } trigger;
    struct : halp::spinbox_f32<"Level", halp::range{-1., 1., 0.}>
    {
      halp_meta(description, "Value the trigger edge crosses")
    } level;
    struct : halp::hslider_f32<"Persistence", halp::range{0., 0.98, 0.}>
    {
      halp_meta(description, "How long traces linger, as on a phosphor screen")
    } persistence;
    struct : halp::spinbox_f32<"Gain", halp::range{-24., 48., 0.}>
    {
      halp_meta(description, "Vertical gain, in dB")
    } gain;
    struct : halp::toggle<"Auto gain">
    {
      halp_meta(description, "Scale to the signal's recent peak")
    } auto_gain;
    struct : halp::spinbox_i32<"X channel", halp::range{0, 63, 0}>
    {
      halp_meta(description, "Channel on the horizontal axis of XY, left channel of the goniometer and correlation")
    } x_channel;
    struct : halp::spinbox_i32<"Y channel", halp::range{0, 63, 1}>
    {
      halp_meta(description, "Channel on the vertical axis of XY, right channel of the goniometer and correlation")
    } y_channel;
    struct : halp::combobox_t<"Goniometer", Polar>
    {
      struct range
      {
        std::string_view values[3]{polar_names[0], polar_names[1], polar_names[2]};
        Polar init{Polar::Lissajous};
      };
    } polar;
  } inputs;

  struct
  {
    struct : halp::val_port<"Correlation", float>
    {
      halp_meta(description, "Phase correlation of the two channels, -1 to 1")
    } correlation;
    struct : halp::val_port<"Balance", float>
    {
      halp_meta(description, "Right minus left RMS level, in dB")
    } balance;
  } outputs;

  std::shared_ptr<ScopeMonitor> monitor = std::make_shared<ScopeMonitor>();
  ossia::meters::correlation corr;
  ossia::meters::rms rms_l, rms_r;
  double rate{};

  //! Also called on the execution thread when the channel count changes:
  //! the followers only need preparing again for a new rate.
  void prepare(halp::setup s)
  {
    const double r = s.rate > 0 ? s.rate : 48000.;
    if(r == rate)
      return;
    rate = r;
    monitor->ring.set_rate(rate);
    corr.prepare(rate, 0.3);
    rms_l.prepare(rate, 0.3);
    rms_r.prepare(rate, 0.3);
  }

  void operator()(int frames)
  {
    const int nch = inputs.audio.channels;
    if(nch <= 0 || frames <= 0)
      return;

    if(monitor->watched())
      monitor->ring.write(nch, frames, [&](int c) { return inputs.audio[c]; });

    const int xc = std::clamp(inputs.x_channel.value, 0, nch - 1);
    const int yc = std::clamp(inputs.y_channel.value, 0, nch - 1);
    corr.process(inputs.audio[xc], inputs.audio[yc], frames);
    rms_l.process(inputs.audio[xc], frames);
    rms_r.process(inputs.audio[yc], frames);
    const float k = float(corr.value());
    monitor->correlation.store(k, std::memory_order_relaxed);
    outputs.correlation.value = k;
    outputs.balance.value = float(
        ossia::detail::LinearGainToDecibels(std::max(rms_r.level(), 1e-6))
        - ossia::detail::LinearGainToDecibels(std::max(rms_l.level(), 1e-6)));
  }
};

struct Layer : public Process::EffectLayerView
{
  static constexpr int max_shown_channels = 8;
  //! Entries of the correlation history per second of signal
  static constexpr double history_per_second = 30.;

  Layer(
      const Process::ProcessModel& process, const Process::Context& doc,
      QGraphicsItem* parent);

  void paint_impl(QPainter* p) const override;
  //! A double click sets the gain to fit what the signal reaches now
  void mousePressEvent(QGraphicsSceneMouseEvent* ev) override;

private:
  Mode mode() const;
  Trigger trigger() const;
  Polar polar() const;
  double gain() const;
  bool autoGain() const;
  float persistence() const;

  void reset();
  void refresh(ScopeMonitor& mon);
  void refreshScope(const Process::SampleRing& ring);
  void refreshDots(const Process::SampleRing& ring);
  void refreshCorrelation(const Process::SampleRing& ring);
  double currentGain() const;
  static void traceInto(
      Ui::Monitor::DensityImage& img, const std::vector<float>& x, double w, double h,
      double g);

  QRectF scopeRect() const;
  QRectF squareRect() const;
  void paintScope(QPainter* p) const;
  static void drawTrace(
      QPainter* p, const QRectF& r, const std::vector<float>& x, double g, QColor col);
  void paintDots(QPainter* p, bool gonio) const;
  void paintCorrelation(QPainter* p, const QRectF& area, bool history = true) const;

  const Process::ControlInlet* m_mode{};
  const Process::ControlInlet* m_window{};
  const Process::ControlInlet* m_trigger{};
  const Process::ControlInlet* m_level{};
  const Process::ControlInlet* m_persistence{};
  const Process::ControlInlet* m_gain{};
  const Process::ControlInlet* m_auto{};
  const Process::ControlInlet* m_x{};
  const Process::ControlInlet* m_y{};
  const Process::ControlInlet* m_polar{};
  Process::MonitorLink* m_link{};
  const score::CommandStackFacade& m_stack;
  Ui::Monitor::DoubleClick m_doubleClick;

  // Oscilloscope: the samples on screen, per channel
  std::vector<std::vector<float>> m_trace;
  std::vector<float> m_search;
  double m_window_ms{};
  bool m_triggered{};
  double m_period{};
  //! Rendered on refresh, drawn as they are
  std::vector<Ui::Monitor::DensityImage> m_phosphor;

  // XY and goniometer: every sample since the last refresh
  Ui::Monitor::DensityImage m_dots;
  std::vector<float> m_xs, m_ys;
  uint64_t m_read{};
  double m_peak{1e-3};
  std::array<float, 90> m_polar_level{};

  std::array<float, 300> m_corr_history{};
  int m_corr_pos{};
  uint64_t m_corr_written{};
  float m_corr{};
  double m_rate{48000.};
  int m_channels{};
};

}
