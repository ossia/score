#pragma once
#include <Process/Monitoring/MonitorLink.hpp>

#include <Effect/EffectLayer.hpp>

#include <ossia/audio/meters.hpp>
#include <Process/Monitoring/Monitor.hpp>

#include <Ui/Monitor/ChannelLayout.hpp>
#include <Ui/Monitor/Paint.hpp>
#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <score_plugin_ui_export.h>

#include <array>
#include <cmath>
#include <memory>
#include <optional>
#include <vector>

namespace Process
{
class ControlInlet;
}

namespace Ui::VUMeter
{
enum class Mode
{
  PeakRms,
  TruePeak,
  VU,
  PpmDin,
  PpmNordic,
  PpmBbc,
  PpmEbu,
  K20,
  K14,
  K12,
  Surround,
  Bits
};

static constexpr int max_channels = 64;

//! What the meter shows at one instant, for every channel. Levels are
//! linear; the view converts them to the scale of the mode.
struct Frame
{
  int channels{};
  Mode mode{};
  //! The level the bar shows
  std::array<float, max_channels> level{};
  //! A second, thinner bar: RMS under the peak, peak beside the RMS
  std::array<float, max_channels> secondary{};
  std::array<float, max_channels> hold{};
  //! Correlation of each channel with the next one around the listener
  std::array<float, max_channels> ring_correlation{};
  //! Bits used by the samples of each channel, as 24-bit integers
  std::array<uint32_t, max_channels> bits{};
  std::array<float, max_channels> dc{};
  uint64_t clipped{};
};

struct MeterMonitor final : Process::Monitor
{
  Process::LatestValue<Frame> frames;
  //! Set by the view to clear the clip indicators
  std::atomic_bool reset_clips{};
};

//! Offset of the K-System scales: their 0 is at -k dBFS.
inline constexpr double kOffset(Mode m) noexcept
{
  return m == Mode::K20 ? 20 : m == Mode::K14 ? 14 : m == Mode::K12 ? 12 : 0;
}

//! From a linear level to the units of a mode's scale: what the bars show
//! and the outlet sends.
inline double toScale(Mode m, double lin, double reference) noexcept
{
  const double db = ossia::detail::LinearGainToDecibels(std::max(lin, 1e-10));
  switch(m)
  {
    case Mode::VU:
    case Mode::PpmNordic:
    case Mode::PpmEbu:
      return db - reference;
    case Mode::PpmDin:
      // Alignment level reads -9 on a DIN meter
      return db - reference - 9.;
    case Mode::PpmBbc:
      // Marks 1 to 7, 4 dB apart, 4 at alignment level
      return 4. + (db - reference) / 4.;
    default:
      return db + kOffset(m);
  }
}

//! The scale a mode is read on: values in its own units, with the colour
//! zones and the marks its standard prints.
struct Scale
{
  double lo{}, hi{};
  double warn{}, over{};
  QString unit;
  std::vector<Ui::Monitor::Tick> ticks;
  Mode mode{};

  double value(double lin, double reference) const noexcept
  {
    return toScale(mode, lin, reference);
  }
};

SCORE_PLUGIN_UI_EXPORT Scale scaleFor(Mode m, double floor, double ceiling);

inline constexpr std::string_view mode_names[]{
    "Peak + RMS", "True peak", "VU",   "PPM DIN", "PPM Nordic", "PPM BBC",
    "PPM EBU",    "K-20",      "K-14", "K-12",    "Surround",   "Bits"};

struct Layer;

struct Node
{
  halp_meta(name, "Audio meter")
  halp_meta(c_name, "VUMeter")
  halp_meta(category, "Monitoring/Audio")
  halp_meta(author, "ossia score")
  halp_meta(manual_url, "")
  halp_meta(
      description,
      "Multichannel level meter: sample peak and RMS, true peak, VU, IEC "
      "programme meters (DIN, Nordic, BBC, EBU), K-System, surround and bit "
      "usage")
  halp_meta(uuid, "0d0a3152-8ee9-4472-8a97-457b8bd6e56a")
  halp_flag(fully_custom_item);
  using Layer = Ui::VUMeter::Layer;

  struct
  {
    halp::dynamic_audio_bus<"Audio", double> audio;

    struct : halp::combobox_t<"Mode", Mode>
    {
      halp_meta(description, "Which meter: each follows its own standard's ballistics and scale.")
      struct range
      {
        std::string_view values[std::size(mode_names)]{
            mode_names[0], mode_names[1], mode_names[2], mode_names[3],
            mode_names[4], mode_names[5], mode_names[6], mode_names[7],
            mode_names[8], mode_names[9], mode_names[10], mode_names[11]};
        Mode init{Mode::PeakRms};
      };
    } mode;

    struct : halp::spinbox_f32<"Reference", halp::range{-40., 0., -18.}>
    {
      halp_meta(
          description,
          "Alignment level in dBFS: what reads 0 VU, TEST on Nordic and EBU "
          "meters, 4 on BBC meters and -9 on DIN meters.")
    } reference;

    struct : halp::spinbox_f32<"Floor", halp::range{-140., -6., -60.}>
    {
      halp_meta(description, "Bottom of the dBFS scale")
    } floor;
    struct : halp::spinbox_f32<"Ceiling", halp::range{-6., 24., 0.}>
    {
      halp_meta(description, "Top of the dBFS scale")
    } ceiling;

    struct : halp::spinbox_f32<"Hold", halp::range{0., 30., 1.5}>
    {
      halp_meta(description, "Seconds a peak stays marked before falling")
    } hold;
    struct : halp::spinbox_f32<"Fall", halp::range{1., 60., 20.}>
    {
      halp_meta(description, "Fall rate of sample and true peak bars, in dB/s")
    } fall;

    halp::impulse_button<"Reset clips"> reset;

    struct : halp::combobox_t<"Layout", Ui::Monitor::ChannelLayout>
    {
      halp_meta(description, "Speaker layout: channel names and surround positions")
      struct range
      {
        std::string_view values[7]{
            Ui::Monitor::channel_layout_names[0], Ui::Monitor::channel_layout_names[1],
            Ui::Monitor::channel_layout_names[2], Ui::Monitor::channel_layout_names[3],
            Ui::Monitor::channel_layout_names[4], Ui::Monitor::channel_layout_names[5],
            Ui::Monitor::channel_layout_names[6]};
        Ui::Monitor::ChannelLayout init{Ui::Monitor::ChannelLayout::Auto};
      };
    } layout;
  } inputs;

  struct
  {
    //! Set about 30 times a second only: each message is a new list
    struct : halp::val_port<"Levels", std::optional<std::vector<float>>>
    {
      halp_meta(description, "Level of each channel on the current scale, about 30 times a second")
    } levels;
    struct : halp::val_port<"Clip", bool>
    {
      halp_meta(description, "Whether any channel went over full scale since the last reset")
    } clip;
  } outputs;

  std::shared_ptr<MeterMonitor> monitor = std::make_shared<MeterMonitor>();

  struct Channel
  {
    ossia::meters::sample_peak peak;
    ossia::meters::rms rms;
    ossia::meters::vu vu;
    ossia::meters::ppm ppm;
    ossia::meters::true_peak tp;
    ossia::meters::peak_hold hold;
    ossia::meters::correlation ring_corr;
    double tp_level{};
    double dc{};
    uint32_t bits{};
  };
  std::vector<Channel> channels = std::vector<Channel>(max_channels);
  //! What every channel's followers start from: their coefficients depend
  //! on the rate only, and are computed once for it
  ossia::meters::ppm ppm_type_i, ppm_type_ii;
  ossia::meters::true_peak tp_proto;

  double rate{};
  Mode current{Mode::PeakRms};
  float current_hold{-1}, current_fall{-1};
  double tp_fall{};
  uint64_t clipped{};
  double since_output{};
  Frame frame;

  //! Also called on the execution thread when the channel count changes:
  //! the followers only need preparing again for a new rate.
  void prepare(halp::setup s)
  {
    const double r = s.rate > 0 ? s.rate : 48000.;
    if(r == rate)
      return;
    rate = r;
    ppm_type_i.prepare(rate, ossia::meters::ppm::type_i);
    ppm_type_ii.prepare(rate, ossia::meters::ppm::type_ii);
    tp_proto.prepare(rate);
    reset_followers();
  }

  void reset_followers() noexcept
  {
    const auto& ppm_proto
        = (current == Mode::PpmDin || current == Mode::PpmNordic) ? ppm_type_i
                                                                     : ppm_type_ii;
    const bool k = current == Mode::K20 || current == Mode::K14 || current == Mode::K12;
    for(auto& c : channels)
    {
      c.rms.prepare(rate, k ? 0.6 : 0.3, k);
      c.vu.prepare(rate);
      c.ppm = ppm_proto;
      c.ppm.env = 0;
      c.tp = tp_proto;
      c.ring_corr.prepare(rate, 0.3);
      c.peak.level = 0;
      c.hold.value = 0;
      c.hold.since = 0;
      c.tp_level = 0;
      c.dc = 0;
      c.bits = 0;
    }
    set_ballistics();
  }

  //! Hold time and fall rate: the peak followers keep their level.
  void set_ballistics() noexcept
  {
    const double fall = ossia::meters::fall_coefficient(inputs.fall.value, rate);
    for(auto& c : channels)
    {
      c.peak.fall = fall;
      c.hold.hold_frames = inputs.hold.value * rate;
      c.hold.fall_per_frame = fall;
    }
    tp_fall = fall;
    current_hold = inputs.hold.value;
    current_fall = inputs.fall.value;
  }

  void operator()(int frames)
  {
    const int nch = std::min(inputs.audio.channels, max_channels);
    if(nch == 0 || frames <= 0)
      return;

    if(inputs.mode.value != current)
    {
      current = inputs.mode.value;
      reset_followers();
    }
    else if(inputs.hold.value != current_hold || inputs.fall.value != current_fall)
    {
      set_ballistics();
    }
    if(inputs.reset || monitor->reset_clips.exchange(false, std::memory_order_relaxed))
      clipped = 0;

    const double dc_coef = 1. - std::exp(-frames / (1. * rate));

    for(int c = 0; c < nch; c++)
    {
      auto& ch = channels[c];
      const double* in = inputs.audio[c];
      ch.peak.process(in, frames);
      double block_peak = 0;
      for(int i = 0; i < frames; i++)
        block_peak = std::max(block_peak, std::abs(in[i]));

      float level{}, secondary{};
      double clip_level = block_peak;
      switch(current)
      {
        case Mode::PeakRms:
        case Mode::Surround:
          ch.rms.process(in, frames);
          level = ch.peak.level;
          secondary = ch.rms.level();
          ch.hold.feed(block_peak, frames);
          break;
        case Mode::TruePeak:
          ch.tp.process(in, frames);
          ch.tp_level = std::max(ch.tp.block_peak, ch.tp_level * std::pow(tp_fall, frames));
          level = ch.tp_level;
          secondary = ch.peak.level;
          clip_level = ch.tp.block_peak;
          ch.hold.feed(ch.tp.block_peak, frames);
          break;
        case Mode::VU:
          ch.vu.process(in, frames);
          level = ch.vu.level();
          ch.hold.feed(level, frames);
          break;
        case Mode::PpmDin:
        case Mode::PpmNordic:
        case Mode::PpmBbc:
        case Mode::PpmEbu:
          ch.ppm.process(in, frames);
          level = ch.ppm.level();
          ch.hold.feed(level, frames);
          break;
        case Mode::K20:
        case Mode::K14:
        case Mode::K12:
          ch.rms.process(in, frames);
          level = ch.rms.level();
          secondary = ch.peak.level;
          ch.hold.feed(block_peak, frames);
          break;
        case Mode::Bits: {
          level = ch.peak.level;
          double sum = 0;
          uint32_t bits = 0;
          for(int i = 0; i < frames; i++)
          {
            sum += in[i];
            const auto q = (int64_t)std::llround(std::clamp(in[i], -1., 1.) * 8388607.);
            bits |= uint32_t(q < 0 ? -q : q);
          }
          ch.bits = bits;
          ch.dc += dc_coef * (sum / frames - ch.dc);
          ch.hold.feed(block_peak, frames);
          break;
        }
      }
      if(clip_level >= 1.)
        clipped |= (uint64_t(1) << c);

      frame.level[c] = level;
      frame.secondary[c] = secondary;
      frame.hold[c] = ch.hold.value;
      frame.bits[c] = ch.bits;
      frame.dc[c] = ch.dc;
    }

    if(current == Mode::Surround)
    {
      const auto r = Ui::Monitor::ring(inputs.layout.value, nch);
      for(int i = 0; i < r.size; i++)
      {
        const int a = r.channel[i], b = r.channel[(i + 1) % r.size];
        auto& corr = channels[a].ring_corr;
        corr.process(inputs.audio[a], inputs.audio[b], frames);
        frame.ring_correlation[i] = corr.value();
      }
    }

    frame.channels = nch;
    frame.mode = current;
    frame.clipped = clipped;
    if(monitor->watched())
      monitor->frames.produce(frame);

    outputs.clip.value = clipped != 0;

    // The levels are for patching: a few lists a second are plenty, and
    // each one is a new message
    since_output += frames;
    if(since_output >= rate / 30.)
    {
      since_output = 0;
      auto& out = outputs.levels.value.emplace(nch);
      for(int c = 0; c < nch; c++)
        out[c] = float(toScale(current, frame.level[c], inputs.reference.value));
    }
  }
};

struct Layer : public Process::EffectLayerView
{
  static constexpr double label_width = 30.;
  //! The mode switch, then the clip indicators
  static constexpr double header = 18.;
  static constexpr double top_margin = 28.;
  static constexpr double bottom_margin = 12.;

  Layer(
      const Process::ProcessModel& process, const Process::Context& doc,
      QGraphicsItem* parent);

  void mousePressEvent(QGraphicsSceneMouseEvent* ev) override;
  void paint_impl(QPainter* p) const override;

private:
  struct StripGeometry
  {
    double start{}, width{}, spacing{};
  };

  Mode mode() const noexcept;
  double reference() const noexcept;
  Ui::Monitor::ChannelLayout layout() const noexcept;
  void updateScale();

  StripGeometry strips(const QRectF& area, int nch) const;
  void drawBars(QPainter* p, const QRectF& area) const;
  void drawNeedles(QPainter* p, const QRectF& area) const;
  void drawBits(QPainter* p, const QRectF& area) const;
  void drawSurround(QPainter* p, const QRectF& r) const;

  const Process::ControlInlet* m_mode{};
  const Process::ControlInlet* m_reference{};
  const Process::ControlInlet* m_floor{};
  const Process::ControlInlet* m_ceiling{};
  const Process::ControlInlet* m_layout{};
  Process::MonitorLink* m_link{};
  Frame m_frame;
  //! Rebuilt when the mode or the range change
  Scale m_scale;
};
}
