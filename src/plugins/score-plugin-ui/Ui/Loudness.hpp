#pragma once
#include <Process/Monitoring/MonitorLink.hpp>

#include <Effect/EffectLayer.hpp>

#include <ossia/audio/meters.hpp>
#include <Process/Monitoring/Monitor.hpp>

#include <QColor>
#include <QString>

#include <Ui/Monitor/ChannelLayout.hpp>
#include <Ui/Monitor/LayerHelpers.hpp>
#include <ebur128.h>
#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <array>
#include <atomic>
#include <cmath>
#include <functional>
#include <memory>
#include <vector>

namespace Process
{
class ControlInlet;
}

namespace Ui::Loudness
{
enum class Mode
{
  Numbers,
  History,
  Histogram,
  Radar
};
inline constexpr std::string_view mode_names[]{"Numbers", "History", "Histogram", "Radar"};

enum class Standard
{
  EbuR128,
  AtscA85,
  Streaming,
  Podcast,
  Custom
};
inline constexpr std::string_view standard_names[]{
    "EBU R128 (-23)", "ATSC A/85 (-24)", "Streaming (-14)", "Podcast (-16)", "Custom (Target)"};

struct Target
{
  double lufs;
  double tolerance;
  double max_true_peak;
};

inline Target targetFor(Standard s, double custom)
{
  switch(s)
  {
    case Standard::EbuR128:
      return {-23., 1., -1.};
    case Standard::AtscA85:
      return {-24., 2., -2.};
    case Standard::Streaming:
      return {-14., 1., -1.};
    case Standard::Podcast:
      return {-16., 1., -1.};
    default:
      return {custom, 1., -1.};
  }
}

enum class Scale
{
  Ebu9,
  Ebu18
};
inline constexpr std::string_view scale_names[]{"+9 scale", "+18 scale"};

static constexpr int max_channels = 16;
//! Short-term loudness histogram: half-LU bins from -70 to +5 LUFS
static constexpr double histogram_low = -70.;
static constexpr double histogram_step = 0.5;
static constexpr int histogram_bins = 150;

struct Readings
{
  //! Increments with every new set of readings, ten per second
  uint64_t sequence{};
  double momentary{-HUGE_VAL};
  double short_term{-HUGE_VAL};
  double integrated{-HUGE_VAL};
  double range{};
  double true_peak{-HUGE_VAL};
  //! Seconds integrated so far
  double duration{};
  bool running{};
  std::array<uint32_t, histogram_bins> histogram{};
};

//! True peak is measured apart, on the channels in use only: libebur128
//! would oversample all of them.
inline ebur128_state* makeState(double rate)
{
  return ebur128_init(
      max_channels, (unsigned long)rate,
      EBUR128_MODE_M | EBUR128_MODE_S | EBUR128_MODE_I | EBUR128_MODE_LRA
          | EBUR128_MODE_HISTOGRAM);
}

/**
 * Besides the readings, the monitor recycles measurement states, as
 * libebur128 has no reset: a reset swaps in a spare state and retires the
 * used one, and recycle(), which the node's worker runs off the execution
 * thread whether or not the process is shown, destroys the retired states
 * and builds new spares. The execution thread never allocates nor frees.
 */
struct LoudnessMonitor final : Process::Monitor
{
  static constexpr int slots = 2;

  Process::LatestValue<Readings> readings;
  std::atomic_bool reset{};
  std::array<std::atomic<ebur128_state*>, slots> spare{};
  std::array<std::atomic<ebur128_state*>, slots> retired{};
  //! The rate spares are built for
  std::atomic<double> rate{48000.};
  //! Set when a recycle() is requested, cleared once it is done
  std::atomic_bool recycling{};

  //! Not on the execution thread: destroys the retired states and fills the
  //! empty spare slots.
  void recycle()
  {
    for(auto& slot : retired)
      if(auto* old = slot.exchange(nullptr))
        ebur128_destroy(&old);
    for(auto& slot : spare)
    {
      if(slot.load())
        continue;
      auto* fresh = makeState(rate.load());
      ebur128_state* expected = nullptr;
      if(!slot.compare_exchange_strong(expected, fresh))
        ebur128_destroy(&fresh);
    }
    recycling = false;
  }

  //! Execution thread, the only one to fill retired slots: one that is
  //! empty and stays so until it fills it, or -1.
  int freeRetiredSlot() const noexcept
  {
    for(int i = 0; i < slots; i++)
      if(!retired[i].load())
        return i;
    return -1;
  }

  /**
   * Execution thread: a spare state for `samplerate`, or null when none is
   * ready. A spare built for another rate is retired on the way, while
   * there is room for it.
   */
  ebur128_state* takeSpare(unsigned long samplerate) noexcept
  {
    for(auto& slot : spare)
    {
      ebur128_state* st = slot.exchange(nullptr);
      if(!st)
        continue;
      if(st->samplerate == samplerate)
        return st;
      if(const int r = freeRetiredSlot(); r >= 0)
        retired[r].store(st);
      else
        slot.store(st);
    }
    return nullptr;
  }

  ~LoudnessMonitor() override
  {
    for(auto* states : {&spare, &retired})
      for(auto& slot : *states)
        if(auto* st = slot.exchange(nullptr))
          ebur128_destroy(&st);
  }
};

inline double lufsToDb(double v)
{
  return std::isfinite(v) ? v : -200.;
}

struct Layer;

struct Node
{
  halp_meta(name, "Loudness")
  halp_meta(c_name, "LoudnessMeter")
  halp_meta(category, "Monitoring/Audio")
  halp_meta(author, "ossia score, libebur128 (Jan Kokemüller)")
  halp_meta(manual_url, "")
  halp_meta(
      description,
      "Loudness per ITU-R BS.1770 and EBU R128: momentary, short-term and "
      "integrated loudness, loudness range and true peak")
  halp_meta(uuid, "a39302dc-502f-479a-b849-8eb0ea4cc7e3")
  halp_flag(fully_custom_item);
  using Layer = Ui::Loudness::Layer;

  struct
  {
    halp::dynamic_audio_bus<"Audio", double> audio;
    struct : halp::combobox_t<"Mode", Mode>
    {
      struct range
      {
        std::string_view values[4]{mode_names[0], mode_names[1], mode_names[2], mode_names[3]};
        Mode init{Mode::Numbers};
      };
    } mode;
    struct : halp::combobox_t<"Standard", Standard>
    {
      halp_meta(description, "What the readings are judged against: the Target, or the target and tolerance of a standard")
      struct range
      {
        std::string_view values[5]{
            standard_names[0], standard_names[1], standard_names[2], standard_names[3],
            standard_names[4]};
        Standard init{Standard::Custom};
      };
    } standard;
    struct : halp::spinbox_f32<"Target", halp::range{-40., 0., -23.}>
    {
      halp_meta(description, "Target in LUFS, when Standard is Custom; a named standard sets its own")
    } custom;
    struct : halp::combobox_t<"Scale", Scale>
    {
      halp_meta(description, "EBU Tech 3341 display range around the target")
      struct range
      {
        std::string_view values[2]{scale_names[0], scale_names[1]};
        Scale init{Scale::Ebu9};
      };
    } scale;
    struct : halp::toggle<"Relative">
    {
      halp_meta(description, "Show LU relative to the target instead of LUFS")
    } relative;
    struct : halp::toggle_t<bool, "Running", halp::toggle_setup{.init = true}>
    {
      halp_meta(description, "Integrate; off pauses the integrated loudness and range")
    } running;
    halp::impulse_button<"Reset"> reset;
    struct : halp::toggle_t<bool, "Follow transport", halp::toggle_setup{.init = true}>
    {
      halp_meta(description, "Start a new integration whenever playback starts")
    } follow;
    struct : halp::combobox_t<"Layout", Ui::Monitor::ChannelLayout>
    {
      halp_meta(description, "Speaker layout: the LFE is left out and surrounds weigh 1.5 dB more")
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
    halp::val_port<"Momentary", float> momentary;
    halp::val_port<"Short-term", float> short_term;
    halp::val_port<"Integrated", float> integrated;
    struct : halp::val_port<"Range", float>
    {
      halp_meta(description, "Loudness range (LRA), in LU")
    } range;
    struct : halp::val_port<"True peak", float>
    {
      halp_meta(description, "Highest true peak of any channel since the last reset, in dBTP")
    } true_peak;
  } outputs;

  std::shared_ptr<LoudnessMonitor> monitor = std::make_shared<LoudnessMonitor>();

  struct worker_t
  {
    std::function<void(std::shared_ptr<LoudnessMonitor>)> request;
    static void work(const std::shared_ptr<LoudnessMonitor>& m) { m->recycle(); }
  } worker;

  struct state_deleter
  {
    void operator()(ebur128_state* st) const noexcept { ebur128_destroy(&st); }
  };
  //! Measures every channel up to max_channels: unused ones are switched off
  //! rather than the state rebuilt, which would allocate
  std::unique_ptr<ebur128_state, state_deleter> state;
  static constexpr int block_frames = 512;
  std::vector<double> interleaved = std::vector<double>(std::size_t(block_frames) * max_channels);
  std::array<ossia::meters::true_peak, max_channels> tp;
  //! Highest true peak of each channel since the last reset, linear
  std::array<double, max_channels> tp_max{};
  int mapped_channels{-1};
  Ui::Monitor::ChannelLayout mapped_layout{};
  double rate{};
  double since_readings{};
  bool restart{};
  //! A reset waits for a spare state: until one is ready, nothing is measured
  bool reset_pending{};
  Readings readings;

  //! Also called on the execution thread when the channel count changes:
  //! the states measure max_channels whatever the input, so only a new rate
  //! rebuilds them.
  void prepare(halp::setup s)
  {
    const double r = s.rate > 0 ? s.rate : 48000.;
    if(r == rate && state)
      return;
    rate = r;
    state.reset(makeState(rate));
    for(auto& t : tp)
      t.prepare(rate);
    tp_max.fill(0.);
    monitor->rate = rate;
    for(auto& slot : monitor->spare)
      if(auto* old = slot.exchange(nullptr))
        ebur128_destroy(&old);
    monitor->recycle();
    mapped_channels = -1;
    readings = {};
  }

  //! Playback started: a new programme
  void start()
  {
    if(inputs.follow)
      restart = true;
  }

  void map_channels(int nch)
  {
    const auto layout = inputs.layout.value;
    if(nch == mapped_channels && layout == mapped_layout)
      return;
    for(int c = 0; c < max_channels; c++)
    {
      int type = EBUR128_UNUSED;
      if(c < nch)
      {
        const double w = Ui::Monitor::loudnessWeight(layout, nch, c);
        type = w == 0. ? EBUR128_UNUSED
               : w > 1. ? EBUR128_LEFT_SURROUND
                        : EBUR128_LEFT;
        // A single channel counts as if heard from both speakers
        if(nch == 1)
          type = EBUR128_DUAL_MONO;
      }
      ebur128_set_channel(state.get(), c, type);
    }
    mapped_channels = nch;
    mapped_layout = layout;
  }

  void request_recycle()
  {
    if(worker.request && !monitor->recycling.exchange(true))
      worker.request(monitor);
  }

  //! Starts over with a spare state; false when none is ready yet.
  bool clear() noexcept
  {
    const int slot = monitor->freeRetiredSlot();
    if(slot < 0)
      return false;
    ebur128_state* fresh = monitor->takeSpare((unsigned long)rate);
    if(!fresh)
      return false;
    monitor->retired[slot].store(state.release());
    state.reset(fresh);
    for(auto& t : tp)
      t.history.fill(0.);
    tp_max.fill(0.);
    mapped_channels = -1;
    readings = {};
    return true;
  }

  void operator()(int frames)
  {
    if(!state || frames <= 0)
      return;
    const int nch = std::min(inputs.audio.channels, max_channels);
    if(nch == 0)
      return;

    if(restart || inputs.reset || monitor->reset.exchange(false, std::memory_order_relaxed))
    {
      restart = false;
      reset_pending = true;
    }
    if(reset_pending)
    {
      const bool done = clear();
      request_recycle();
      if(!done)
        return;
      reset_pending = false;
    }
    map_channels(nch);

    const bool running = inputs.running.value;
    readings.running = running;
    if(running)
    {
      for(int done = 0; done < frames;)
      {
        const int n = std::min(frames - done, block_frames);
        std::fill_n(interleaved.begin(), std::size_t(n) * max_channels, 0.);
        for(int c = 0; c < nch; c++)
        {
          const double* in = inputs.audio[c] + done;
          for(int i = 0; i < n; i++)
            interleaved[std::size_t(i) * max_channels + c] = in[i];
        }
        ebur128_add_frames_double(state.get(), interleaved.data(), n);
        done += n;
      }
      for(int c = 0; c < nch; c++)
      {
        tp[c].process(inputs.audio[c], frames);
        tp_max[c] = std::max(tp_max[c], tp[c].block_peak);
      }
      readings.duration += frames / rate;
    }

    since_readings += frames;
    if(since_readings < rate / 10.)
      return;
    since_readings = 0;

    auto* st = state.get();
    double v{};
    if(ebur128_loudness_momentary(st, &v) == EBUR128_SUCCESS)
      readings.momentary = v;
    if(ebur128_loudness_shortterm(st, &v) == EBUR128_SUCCESS)
      readings.short_term = v;
    if(ebur128_loudness_global(st, &v) == EBUR128_SUCCESS)
      readings.integrated = v;
    if(ebur128_loudness_range(st, &v) == EBUR128_SUCCESS)
      readings.range = v;
    double peak = 0;
    for(int c = 0; c < nch; c++)
      peak = std::max(peak, tp_max[c]);
    readings.true_peak = ossia::detail::LinearGainToDecibels(std::max(peak, 1e-10));
    if(running && std::isfinite(readings.short_term) && readings.short_term > histogram_low)
    {
      const int bin = int((readings.short_term - histogram_low) / histogram_step);
      if(bin >= 0 && bin < histogram_bins)
        readings.histogram[bin]++;
    }
    readings.sequence++;
    if(monitor->watched())
      monitor->readings.produce(readings);

    outputs.momentary.value = float(lufsToDb(readings.momentary));
    outputs.short_term.value = float(lufsToDb(readings.short_term));
    outputs.integrated.value = float(lufsToDb(readings.integrated));
    outputs.range.value = float(readings.range);
    outputs.true_peak.value = float(readings.true_peak);
  }
};

struct Layer : public Process::EffectLayerView
{
  struct Point
  {
    float momentary, short_term;
  };

  Layer(
      const Process::ProcessModel& process, const Process::Context& doc,
      QGraphicsItem* parent);

  void mousePressEvent(QGraphicsSceneMouseEvent* ev) override;
  void paint_impl(QPainter* p) const override;

private:
  Mode mode() const;
  Target target() const;
  bool relative() const;
  std::pair<double, double> range() const;
  QString format(double lufs) const;
  QColor zone(double lufs) const;
  QRectF resetRect() const;

  void paintNumbers(QPainter* p, const QRectF& r) const;
  void paintHistory(QPainter* p, const QRectF& area) const;
  void paintHistogram(QPainter* p, const QRectF& area) const;
  void paintRadar(QPainter* p, const QRectF& area) const;

  const Process::ControlInlet* m_mode{};
  const Process::ControlInlet* m_standard{};
  const Process::ControlInlet* m_custom{};
  const Process::ControlInlet* m_scale{};
  const Process::ControlInlet* m_relative{};
  Process::MonitorLink* m_link{};
  Ui::Monitor::DoubleClick m_doubleClick;
  Readings m_now;
  //! Ten readings per second, the last ten minutes
  std::vector<Point> m_history = std::vector<Point>(6000);
  int m_history_pos{};
  int m_history_count{};
};
}
