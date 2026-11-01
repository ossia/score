#pragma once
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <Process/Dataflow/PortItem.hpp>
#include <Process/Monitoring/EventRing.hpp>
#include <Process/Monitoring/MonitorLink.hpp>
#include <Process/Process.hpp>

#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <Effect/EffectLayer.hpp>
#include <Effect/EffectLayout.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/model/Skin.hpp>

#include <Process/Monitoring/Monitor.hpp>
#include <ossia/detail/flat_map.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QFontMetrics>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>

#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/midi.hpp>

#include <Ui/Monitor/LayerHelpers.hpp>
#include <Ui/Monitor/Ring.hpp>
#include <Ui/Monitor/MidiText.hpp>
#include <Ui/Monitor/Paint.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <optional>
#include <vector>

/**
 * @brief MIDI display: a scrolling monitor for whatever goes through a MIDI port.
 *
 * Cable any MIDI outlet to it to see the raw message stream; outlets fan out,
 * so it taps a chain without interrupting it. It is primarily meant to debug
 * note lifetime issues: a note whose note-off is never sent is drawn as a bar
 * that never ends, in red, and counted in the "stuck" readout in the header.
 * Retriggers without an intervening note-off, and note-offs for notes that were
 * not held, are also flagged, as those are the usual symptoms of a broken
 * note-off path.
 *
 * Implementation notes:
 *
 * - Messages reach the view through an event ring on the process monitor,
 *   none lost while the view keeps up; the node also publishes which notes are
 *   held, so that a view opened during playback starts from the right state.
 *
 * - Note pairing (held / stuck / retriggered / unmatched note-off) is done in
 *   the node, not in the layer, and shipped as a per-event flag. The layer only
 *   only holds what came while it was shown, and is rebuilt from scratch when
 *   playback restarts; deriving pairing from such a window reports the note-ons
 *   whose note-off fell before it as stuck, and the note-offs whose note-on fell
 *   before it as unmatched. Both are artifacts of the window, not of the stream.
 *
 * - Caveat: avendish drops incoming messages whose timestamp falls outside
 *   [tick_start; tick_start + frames[ (see port_run_preprocess.hpp). Messages
 *   emitted by an upstream node with an out-of-tick timestamp are therefore
 *   invisible here as well as to every other avnd process.
 */
namespace Ui::MidiDisplay
{
enum class Mode
{
  PianoRoll,
  EventList,
  Controllers,
  Activity,
  Expression
};
enum class NotePalette
{
  Green,
  Inferno,
  Viridis,
  Magma,
  Turbo,
  Channels
};

inline constexpr std::string_view mode_names[]{
    "Piano roll", "Event list", "Controllers", "Activity", "Expression"};

//! One message as the node saw it.
struct MidiEvent
{
  uint64_t seq{};
  double t{};
  uint8_t status{};
  uint8_t d1{};
  uint8_t d2{};
  uint8_t flags{};
  //! Total size of the message: more than 3 for a SysEx
  uint32_t size{};
};

//! Which notes are held on each channel.
struct HeldNotes
{
  std::array<std::array<uint64_t, 2>, 16> bits{};
  int held{};
  int stuck{};
  double now{};
  double window{};

  bool test(int chan, int pitch) const noexcept
  {
    return bits[chan][pitch / 64] & (uint64_t(1) << (pitch % 64));
  }
};

struct MidiMonitor final : Process::Monitor
{
  Process::EventRing<MidiEvent, 4096> events;
  Process::LatestValue<HeldNotes> held;
};

struct Node
{
  halp_meta(name, "MIDI display")
  halp_meta(c_name, "MidiDisplay")
  halp_meta(category, "Monitoring/Midi")
  halp_meta(author, "ossia score")
  halp_meta(manual_url, "")
  halp_meta(description, "Visualize the MIDI messages going through a port")
  halp_meta(uuid, "a2b6c7f1-3d54-4e9a-9c21-8f0d5e73b104")
  halp_flag(fully_custom_item);

  static double recommended_height() { return 150.; }

  struct
  {
    halp::midi_bus<"in"> midi;
    //! How much of the past is shown: seconds, or a note value (bars).
    halp::time_chooser<"Window", halp::range{0.5, 60., 8.}> window;

    struct : halp::combobox_t<"Mode", Mode>
    {
      struct range
      {
        std::string_view values[5]{
            mode_names[0], mode_names[1], mode_names[2], mode_names[3], mode_names[4]};
        Mode init{Mode::PianoRoll};
      };
    } mode;
    struct : halp::spinbox_i32<"Channel", halp::range{0, 16, 0}>
    {
      halp_meta(description, "Show only this channel; 0 shows all of them")
    } channel;
    struct : halp::toggle_t<bool, "Hide clock", halp::toggle_setup{.init = true}>
    {
      halp_meta(description, "Leave timing clock and active sensing out of the event list")
    } hide_clock;
    struct : halp::combobox_t<"Palette", NotePalette>
    {
      halp_meta(description, "Colours of the notes: by velocity, or by channel")
      struct range
      {
        std::string_view values[6]{"Green", "Inferno", "Viridis", "Magma", "Turbo", "Channels"};
        NotePalette init{NotePalette::Green};
      };
    } palette;
  } inputs;

  struct
  {
    // Not written: the view reads the monitor. Declared so that documents
    // keep the outlet they were saved with.
    struct : halp::val_port<"events", std::optional<std::vector<float>>>
    {
      enum widget
      {
        control
      };
    } events;
  } outputs;

  static constexpr double heartbeat_seconds = 0.03;
  // A note held for longer than this without a note-off is reported as stuck.
  static constexpr double stuck_after_seconds = 2.;

  enum event_flag : int
  {
    flag_none = 0,
    flag_retrigger = 1, // note-on while the same note was already held
    flag_orphan = 2,    // note-off for a note that was not held
  };

  struct held_note
  {
    bool active{};
    float on{};
  };

  std::shared_ptr<MidiMonitor> monitor = std::make_shared<MidiMonitor>();
  double m_rate = 48000.;
  int64_t m_frames = 0;
  double m_last_push = -1e9;
  uint64_t m_seq = 0;
  // Pairing is done here rather than in the layer: this sees every message from
  // the first one on, while the layer only sees what came while it was shown.
  std::array<std::array<held_note, 128>, 16> m_held_notes{};
  HeldNotes m_held_state;

  void prepare(halp::setup s) noexcept
  {
    m_rate = s.rate > 0 ? s.rate : 48000.;
    m_frames = 0;
    m_last_push = -1e9;
    m_seq = 0;
    m_held_notes = {};
  }

  using tick = halp::tick_musical;
  void operator()(halp::tick_musical t)
  {
    const double t0 = m_frames / m_rate;
    const double t1 = (m_frames + t.frames) / m_rate;
    m_frames += t.frames;

    const bool watched = monitor->watched();
    bool got_message = false;
    for(const auto& m : inputs.midi)
    {
      if(m.bytes.empty())
        continue;

      const double ts = t0 + double(m.timestamp) / m_rate;
      const auto n = m.bytes.size();
      const uint8_t status = m.bytes[0];
      const uint8_t d1 = n > 1 ? m.bytes[1] : 0;
      const uint8_t d2 = n > 2 ? m.bytes[2] : 0;
      const auto flags = uint8_t(track(status, d1, d2, float(ts)));
      if(watched)
        monitor->events.push({m_seq, ts, status, d1, d2, flags, uint32_t(n)});
      m_seq++;
      got_message = true;
    }

    if(watched && (got_message || (t1 - m_last_push) >= heartbeat_seconds))
    {
      auto& h = m_held_state;
      h = {};
      for(int chan = 0; chan < 16; chan++)
      {
        for(int pitch = 0; pitch < 128; pitch++)
        {
          const auto& note = m_held_notes[chan][pitch];
          if(!note.active)
            continue;
          h.bits[chan][pitch / 64] |= uint64_t(1) << (pitch % 64);
          h.held++;
          h.stuck += (t1 - note.on) > stuck_after_seconds;
        }
      }
      h.now = t1;
      h.window = inputs.window.value;
      monitor->held.produce(h);
      m_last_push = t1;
    }
  }

  //! Pairs a message against the running held-note state, returning what is
  //! anomalous about it. Malformed messages are not paired.
  int track(uint8_t status, uint8_t d1, uint8_t d2, float t) noexcept
  {
    using T = libremidi::message_type;
    const int chan = status & 0x0F;
    switch(Ui::Monitor::midiMessageType(status, d1, d2))
    {
      case T::NOTE_ON: {
        auto& note = m_held_notes[chan][d1];
        const bool was_held = note.active;
        note = {true, t};
        return was_held ? flag_retrigger : flag_none;
      }
      case T::NOTE_OFF: {
        auto& note = m_held_notes[chan][d1];
        if(!note.active)
          return flag_orphan;
        note = {};
        return flag_none;
      }
      default:
        return flag_none;
    }
  }

  //! From time to x, the newest at the right: on a pixel grid fixed in time,
//! shifted by whole pixels as time passes, so that everything scrolls
//! together and keeps its width. Placing each edge on its own makes
//! neighbours round differently and shake by a pixel.
struct TimeAxis
{
  double x0, w, px_per_s, right;
  TimeAxis(double now, double win, double x0, double w)
      : x0{x0}
      , w{w}
      , px_per_s{w / win}
      , right{std::floor(now * px_per_s)}
  {
  }
  double operator()(double t) const noexcept
  {
    return std::clamp(x0 + w - (right - std::floor(t * px_per_s)), x0, x0 + w);
  }
};

struct Layer : public Process::EffectLayerView
  {
  public:
    static constexpr double left_gutter = 34.;
    static constexpr double header_height = 18.;
    static constexpr double system_lane_height = 9.;
    static constexpr int default_low_pitch = 48;
    static constexpr int default_high_pitch = 72;
    static constexpr int max_log = 4096;

    using Event = MidiEvent;

    struct Note
    {
      double on{};
      double off{};
      uint8_t chan{};
      uint8_t pitch{};
      uint8_t vel{};
      bool retriggered{}; // note-on while already held: the note-off went missing
    };

    struct OpenNote
    {
      bool active{};
      double on{};
      uint8_t vel{};
    };

    struct OrphanOff
    {
      double t{};
      uint8_t chan{};
      uint8_t pitch{};
    };

    //! A continuous controller over time: CC, pitch bend, pressure
    struct Lane
    {
      boost::circular_buffer<std::pair<double, float>> points;
    };

    Scenario::IntervalModel* m_interval{};
    Process::ControlInlet* m_window_inlet{};
    const Process::ControlInlet* m_mode_inlet{};
    const Process::ControlInlet* m_channel_inlet{};
    const Process::ControlInlet* m_clock_inlet{};
    const Process::ControlInlet* m_palette_inlet{};
    score::QGraphicsCombo* m_switch{};
    Process::MonitorLink* m_link{};
    HeldNotes m_held_bits;
    //! Key: channel * 256 + controller; 128 is pitch bend, 129 channel pressure
    ossia::flat_map<int, Lane> m_lanes;
    //! Per channel: when it last received something, and what
    std::array<double, 16> m_last_activity{};
    std::array<uint8_t, 16> m_last_status{};
    //! Per channel, the state an MPE note is shaped by
    std::array<float, 16> m_bend{};
    std::array<float, 16> m_timbre{};
    //! Per channel and note: velocity, then channel or polyphonic pressure
    std::array<std::array<float, 128>, 16> m_note_pressure{};
    struct ExpressionPoint
    {
      double t;
      float pitch;
      float pressure;
      float timbre;
    };
    //! Per channel and note, the path of a sounding MPE note
    ossia::flat_map<int, boost::circular_buffer<ExpressionPoint>> m_paths;

    //! A message and its text, formatted once when it arrives
    struct LogEntry
    {
      Event e;
      QString time, text, bytes;
    };
    boost::circular_buffer<LogEntry> m_log;
    boost::circular_buffer<Note> m_notes;
    boost::circular_buffer<OrphanOff> m_orphans;
    std::array<std::array<OpenNote, 128>, 16> m_open{};

    //! Reused by the painting, which would otherwise allocate one per lane and note
    mutable QPolygonF m_polyline;

    double m_now = 0.;
    // Held / stuck counts come from the node: it has seen the whole stream.
    double m_window = 0.;
    int m_held = 0;
    int m_stuck = 0;

    Layer(
        const Process::ProcessModel& process, const Process::Context& doc,
        QGraphicsItem* parent)
        : Process::EffectLayerView{parent}
        , m_interval{Scenario::closestParentInterval(process.parent())}
    {
      setAcceptedMouseButtons({});
      setToolTip(tr("Green: a note that was played and released.\n"
                    "Amber, running to the right edge: currently held.\n"
                    "Red hatched: held for more than 2s without a note-off.\n"
                    "Orange: a note-on while that note was already held.\n"
                    "Magenta cross: a note-off for a note that was not held.\n"
                    "Bottom lane: non-note messages.\n"
                    "Right-click to copy the message log."));

      Ui::Monitor::showInlet(process, 0, doc, this, this);

      m_window_inlet = static_cast<Process::ControlInlet*>(process.inlets()[1]);
      connect(
          m_window_inlet, &Process::ControlInlet::valueChanged, this,
          [this](const ossia::value&) { update(); });

      if(m_interval)
      {
        connect(
            m_interval, &Scenario::IntervalModel::executionEvent, this,
            [this](Scenario::IntervalExecutionEvent ev) {
          switch(ev)
          {
            case Scenario::IntervalExecutionEvent::Stopped:
              reset();
              break;
            default:
              break;
          }
        });
      }

      m_mode_inlet = Ui::Monitor::controlInlet(process, 2);
      m_switch = Ui::Monitor::ModeSwitch::make(m_mode_inlet, doc, this, this);
      m_channel_inlet = Ui::Monitor::controlInlet(process, 3);
      m_clock_inlet = Ui::Monitor::controlInlet(process, 4);
      m_palette_inlet = Ui::Monitor::controlInlet(process, 5);
      Ui::Monitor::repaintOn(this, m_mode_inlet, m_channel_inlet, m_clock_inlet, m_palette_inlet);

      m_link = new Process::MonitorLink{
          process, *this,
          [this](Process::Monitor& m) { refresh(static_cast<MidiMonitor&>(m)); },
          [this](Process::Monitor*) { reset(); }, this};
    }

    Mode mode() const
    {
      return Ui::Monitor::enumValue(m_mode_inlet, Mode::PianoRoll, Mode::Expression);
    }
    //! The channel shown, 0 to 15, or -1 for all
    int channelFilter() const
    {
      return std::clamp(int(Ui::Monitor::floatValue(m_channel_inlet, 0.f)), 0, 16) - 1;
    }
    bool shows(const Event& e) const
    {
      const int f = channelFilter();
      if(f >= 0 && ((e.status & 0xF0) == 0xF0 || (e.status & 0x0F) != f))
        return false;
      return true;
    }

    double window() const noexcept
    {
      if(!m_window_inlet)
        return 8.;
      // The node's, once it has sent one: a synced window is only in seconds
      // there. Until then, the free value of the port.
      const double w = m_window > 0.
                           ? m_window
                           : ossia::convert<ossia::vec2f>(m_window_inlet->value())[0];
      return std::isfinite(w) && w > 0.05 ? w : 8.;
    }

    void reset()
    {
      m_log.clear();
      m_notes.clear();
      m_orphans.clear();
      m_open = {};
      m_now = 0.;
      m_held = 0;
      m_stuck = 0;
      m_held_bits = {};
      m_lanes.clear();
      m_paths.clear();
      m_last_activity.fill(-1e9);
      m_bend.fill(0.f);
      m_timbre.fill(0.f);
      m_note_pressure = {};
      update();
    }

    void refresh(MidiMonitor& mon)
    {
      bool changed = false;
      HeldNotes h;
      if(mon.held.consume(h))
      {
        // The node restarted (new playback): our timeline is stale
        if(h.now < m_now)
          reset();
        m_held_bits = h;
        m_now = h.now;
        m_held = h.held;
        m_stuck = h.stuck;
        m_window = h.window;
        changed = true;
      }
      mon.events.drain([&](const Event& e) {
        apply(e);
        track(e);
        Ui::Monitor::pushRing(
            m_log,
            LogEntry{
                e, QString::number(e.t, 'f', 3),
                Ui::Monitor::describeMidi(e.status, e.d1, e.d2, e.size, true),
                Ui::Monitor::midiBytesText(e.status, e.d1, e.d2, e.size)},
            max_log);
        changed = true;
      });
      if(changed)
      {
        trim();
        update();
      }
    }

    //! Controllers, activity and MPE expression.
    void track(const Event& e)
    {
      using T = libremidi::message_type;
      const auto type = Ui::Monitor::midiMessageType(e.status, e.d1, e.d2);
      if(!Ui::Monitor::isChannelMessage(type))
        return;
      const int chan = e.status & 0x0F;
      m_last_activity[chan] = e.t;
      m_last_status[chan] = e.status;

      auto lane = [&](int key, float v) {
        auto& l = m_lanes[chan * 256 + key];
        Ui::Monitor::pushRing(l.points, std::pair{e.t, v});
      };
      auto& pressure = m_note_pressure[chan];
      // A point on the path of a sounding note, from its current expression
      auto point = [&](int pitch) {
        if(m_open[chan][pitch].active)
          Ui::Monitor::pushRing(
              m_paths[chan * 128 + pitch],
              ExpressionPoint{
                  e.t, float(pitch + m_bend[chan] * mpe_bend_range), pressure[pitch],
                  m_timbre[chan]});
      };
      // What applies to the whole channel shapes every note sounding on it
      auto every_note = [&] {
        for(int pitch = 0; pitch < 128; pitch++)
          point(pitch);
      };

      switch(type)
      {
        case T::CONTROL_CHANGE:
          lane(e.d1, e.d2 / 127.f);
          if(e.d1 == 74)
          {
            m_timbre[chan] = e.d2 / 127.f;
            every_note();
          }
          break;
        case T::PITCH_BEND: {
          const float bend = (int(e.d1) + (int(e.d2) << 7) - 8192) / 8192.f;
          lane(128, bend * 0.5f + 0.5f);
          m_bend[chan] = bend;
          every_note();
          break;
        }
        case T::AFTERTOUCH:
          lane(129, e.d1 / 127.f);
          pressure.fill(e.d1 / 127.f);
          every_note();
          break;
        case T::POLY_PRESSURE:
          pressure[e.d1] = e.d2 / 127.f;
          point(e.d1);
          break;
        case T::NOTE_ON:
          m_paths[chan * 128 + e.d1].clear();
          pressure[e.d1] = e.d2 / 127.f;
          point(e.d1);
          break;
        default:
          break;
      }
    }

    //! Semitones of a full pitch bend for an MPE member channel
    static constexpr float mpe_bend_range = 48.f;

    // The flags are computed by the node against the full stream. This only
    // turns them into geometry: an unmatched message here means our window
    // simply does not reach far enough back, which is not an anomaly.
    void apply(const Event& e)
    {
      using T = libremidi::message_type;
      const int chan = e.status & 0x0F;
      switch(Ui::Monitor::midiMessageType(e.status, e.d1, e.d2))
      {
        case T::NOTE_ON: {
          auto& open = m_open[chan][e.d1];
          if(open.active)
          {
            Ui::Monitor::pushRing(
                m_notes, Note{open.on, e.t, uint8_t(chan), e.d1, open.vel,
                              bool(e.flags & flag_retrigger)});
          }
          open = {true, e.t, e.d2};
          break;
        }
        case T::NOTE_OFF: {
          if(e.flags & flag_orphan)
          {
            Ui::Monitor::pushRing(m_orphans, OrphanOff{e.t, uint8_t(chan), e.d1});
            break;
          }

          auto& open = m_open[chan][e.d1];
          if(open.active)
          {
            Ui::Monitor::pushRing(
                m_notes, Note{open.on, e.t, uint8_t(chan), e.d1, open.vel, false});
            open = {};
          }
          break;
        }
        default:
          break;
      }
    }

    void trim()
    {
      // Keep a bit more than the largest possible window so that changing the
      // window slider does not blank the past.
      const double horizon = m_now - 61.;
      while(!m_notes.empty() && m_notes.front().off < horizon)
        m_notes.pop_front();
      while(!m_orphans.empty() && m_orphans.front().t < horizon)
        m_orphans.pop_front();
      for(auto& [k, l] : m_lanes)
        while(l.points.size() > 1 && l.points[1].first < horizon)
          l.points.pop_front();
      for(auto it = m_paths.begin(); it != m_paths.end();)
      {
        while(!it->second.empty() && it->second.front().t < horizon)
          it->second.pop_front();
        it = it->second.empty() ? m_paths.erase(it) : std::next(it);
      }
    }

    int held_count() const noexcept { return m_held; }
    int stuck_count() const noexcept { return m_stuck; }

    QString logText() const
    {
      QString out;
      for(const auto& l : m_log)
      {
        out += QString::asprintf("%9.4f  ", double(l.e.t));
        out += l.text;
        out += QLatin1Char('\n');
      }
      return out;
    }

    static QColor kind_color(libremidi::message_type k) noexcept
    {
      using T = libremidi::message_type;
      switch(k)
      {
        case T::CONTROL_CHANGE:
          return QColor(90, 160, 230);
        case T::PITCH_BEND:
          return QColor(180, 130, 230);
        case T::PROGRAM_CHANGE:
          return QColor(230, 190, 90);
        case T::POLY_PRESSURE:
        case T::AFTERTOUCH:
          return QColor(120, 200, 190);
        default:
          return QColor(150, 150, 150);
      }
    }

    void paint_impl(QPainter* p) const override
    {
      switch(mode())
      {
        case Mode::PianoRoll:
          paintRoll(p, false);
          break;
        case Mode::Expression:
          paintRoll(p, true);
          break;
        case Mode::EventList:
          paintList(p);
          break;
        case Mode::Controllers:
          paintLanes(p);
          break;
        case Mode::Activity:
          paintActivity(p);
          break;
      }
    }

    void paintRoll(QPainter* p, bool expression) const
    {
      const auto rect = boundingRect();
      const double W = rect.width();
      const double H = rect.height();
      if(W <= left_gutter + 8. || H <= header_height + system_lane_height + 8.)
        return;

      const double roll_x = left_gutter;
      const double roll_w = W - left_gutter - 2.;
      const double roll_y = header_height;
      const double roll_h = H - header_height - system_lane_height;

      const double win = window();
      const double t_min = m_now - win;

      const auto to_x = TimeAxis(m_now, win, roll_x, roll_w);

      // Vertical pitch range: fit what is on screen, with an octave minimum
      int lo = 127, hi = 0;
      const auto extend = [&](int pitch) {
        lo = std::min(lo, pitch);
        hi = std::max(hi, pitch);
      };
      for(const auto& n : m_notes)
        if(n.off >= t_min)
          extend(n.pitch);
      for(const auto& chan : m_open)
        for(int pitch = 0; pitch < 128; pitch++)
          if(chan[pitch].active)
            extend(pitch);

      if(lo > hi)
      {
        lo = default_low_pitch;
        hi = default_high_pitch;
      }
      lo = std::max(0, lo - 1);
      hi = std::min(127, hi + 1);
      if(hi - lo < 12)
      {
        const int mid = (lo + hi) / 2;
        lo = std::max(0, mid - 6);
        hi = std::min(127, lo + 12);
      }

      const int rows = hi - lo + 1;
      const double row_h = roll_h / rows;
      const auto to_y = [&](int pitch) { return roll_y + (hi - pitch) * row_h; };

      p->save();
      p->setRenderHint(QPainter::Antialiasing, false);

      draw_grid(p, roll_x, roll_w, row_h, lo, hi, to_y);
      if(expression)
        draw_expression(p, row_h, t_min, to_x, to_y);
      else
        draw_notes(p, row_h, t_min, to_x, to_y);
      draw_orphans(p, row_h, t_min, to_x, to_y);
      draw_system_lane(p, roll_x, H - system_lane_height, roll_w, t_min, to_x);
      draw_header(p, W);

      p->restore();
    }

  private:
    void draw_grid(
        QPainter* p, double roll_x, double roll_w, double row_h, int lo, int hi,
        auto to_y) const
    {
      QFont f = Ui::Monitor::smallFont();
      p->setFont(f);

      for(int pitch = lo; pitch <= hi; pitch++)
      {
        if(pitch % 12 != 0)
          continue;

        const double line_y = to_y(pitch) + row_h;

        p->setPen(QColor(72, 72, 80));
        p->drawLine(QPointF(roll_x, line_y), QPointF(roll_x + roll_w, line_y));

        // Centred on the line itself
        p->setPen(QColor(140, 140, 150));
        constexpr double h = 10.;
        p->drawText(
            QRectF(0, line_y - h / 2., roll_x - 4, h), Qt::AlignRight | Qt::AlignVCenter,
            Ui::Monitor::midiNoteName(pitch));
      }
    }

    void draw_notes(
        QPainter* p, double row_h, double t_min, auto to_x, auto to_y) const
    {
      p->setPen(Qt::NoPen);
      const double bar_h = std::max(1., row_h - 1.);

      const int filter = channelFilter();
      for(const auto& n : m_notes)
      {
        if(n.off < t_min || (filter >= 0 && n.chan != filter))
          continue;
        const double x0 = to_x(n.on);
        const double x1 = std::max(to_x(n.off), x0 + 1.);
        QColor col = n.retriggered ? QColor(240, 160, 40) : note_color(n.vel, n.chan);
        p->fillRect(QRectF(x0, to_y(n.pitch), x1 - x0, bar_h), col);
      }

      // Notes still held: they run all the way to "now". If they have been held
      // for longer than the threshold, this is the missing-note-off symptom.
      for(int chan = 0; chan < 16; chan++)
      {
        if(filter >= 0 && chan != filter)
          continue;
        for(int pitch = 0; pitch < 128; pitch++)
        {
          const auto& open = m_open[chan][pitch];
          if(!open.active)
            continue;

          const double x0 = to_x(open.on);
          const double x1 = to_x(m_now);
          const bool stuck = (m_now - open.on) > stuck_after_seconds;
          const QRectF r(x0, to_y(pitch), std::max(1., x1 - x0), bar_h);

          if(stuck)
          {
            p->fillRect(r, QBrush(QColor(220, 40, 40), Qt::BDiagPattern));
            p->fillRect(QRectF(r.left(), r.top(), 2., r.height()), QColor(255, 80, 80));
          }
          else
          {
            p->fillRect(r, QColor(230, 190, 60));
          }
        }
      }
    }

    void draw_orphans(
        QPainter* p, double row_h, double t_min, auto to_x, auto to_y) const
    {
      p->setPen(QPen(QColor(255, 90, 200), 1.));
      const double s = std::min(4., std::max(2., row_h));
      for(const auto& o : m_orphans)
      {
        if(o.t < t_min)
          continue;
        const double x = to_x(o.t);
        const double y = to_y(o.pitch) + row_h / 2.;
        p->drawLine(QPointF(x - s, y - s), QPointF(x + s, y + s));
        p->drawLine(QPointF(x - s, y + s), QPointF(x + s, y - s));
      }
    }

    void draw_system_lane(
        QPainter* p, double x0, double y, double w, double t_min, auto to_x) const
    {
      p->setPen(QColor(72, 72, 80));
      p->drawLine(QPointF(x0, y), QPointF(x0 + w, y));

      using T = libremidi::message_type;
      const auto first = std::lower_bound(
          m_log.begin(), m_log.end(), t_min,
          [](const LogEntry& l, double t) { return l.e.t < t; });
      for(auto it = first; it != m_log.end(); ++it)
      {
        const auto& e = it->e;
        const auto k = Ui::Monitor::midiMessageType(e.status, e.d1, e.d2);
        if(k == T::NOTE_ON || k == T::NOTE_OFF)
          continue;
        p->setPen(kind_color(k));
        const double x = to_x(e.t);
        p->drawLine(QPointF(x, y + 1.), QPointF(x, y + system_lane_height - 1.));
      }
    }

    void draw_header(QPainter* p, double W) const
    {
      const QFont f = Ui::Monitor::smallFont();
      p->setFont(f);

      const QFontMetricsF fm{f};
      const auto band = [&](double x, double w) { return QRectF(x, 0, w, header_height); };
      constexpr double gap = 8.;

      // After the inlet and the mode switch
      double x = m_switch ? m_switch->x() + m_switch->boundingRect().width() + gap
                          : Ui::Monitor::ModeSwitch::position.x();

      // Stuck notes are a problem to see at once; the rest shows on hover
      const bool hovered = isUnderMouse();
      if(hovered)
      {
        const auto held = QStringLiteral("held %1").arg(held_count());
        p->setPen(QColor(160, 160, 168));
        p->drawText(band(x, fm.horizontalAdvance(held)), Qt::AlignVCenter, held);
        x += fm.horizontalAdvance(held) + gap;
      }

      if(const int stuck = stuck_count(); stuck > 0)
      {
        const auto txt = QStringLiteral("STUCK %1").arg(stuck);
        p->setPen(QColor(255, 90, 90));
        p->drawText(band(x, fm.horizontalAdvance(txt)), Qt::AlignVCenter, txt);
        x += fm.horizontalAdvance(txt) + gap;
      }

      if(hovered && !m_log.empty())
      {
        const auto& txt = m_log.back().text;
        const double avail = W - 2. - x;
        if(fm.horizontalAdvance(txt) <= avail)
        {
          p->setPen(QColor(130, 130, 140));
          p->drawText(band(x, avail), Qt::AlignRight | Qt::AlignVCenter, txt);
        }
      }
    }

    //! MPE: each note follows its pitch bend, thick with pressure, coloured
    //! by timbre (CC 74). Pressure and timbre are drawn in steps, so that a
    //! run of points with the same expression is one polyline and one pen.
    void draw_expression(QPainter* p, double row_h, double t_min, auto to_x, auto to_y) const
    {
      p->setRenderHint(QPainter::Antialiasing, true);
      constexpr int steps = 32;
      const auto quantize = [](float v) { return int(std::clamp(v, 0.f, 1.f) * (steps - 1) + 0.5f); };
      const auto pen_for = [](int timbre, int pressure) {
        const double t = double(timbre) / (steps - 1), w = double(pressure) / (steps - 1);
        return QPen(
            QColor::fromHsvF(0.6 - 0.6 * t, 0.8, 0.95), 1. + 6. * w, Qt::SolidLine,
            Qt::RoundCap);
      };
      const int filter = channelFilter();
      auto& run = m_polyline;
      for(const auto& [key, path] : m_paths)
      {
        const int chan = key / 128;
        if(path.empty() || (filter >= 0 && chan != filter))
          continue;
        // From the last point before the window, which still shapes its left edge
        auto it = std::lower_bound(
            path.begin(), path.end(), t_min,
            [](const ExpressionPoint& pt, double t) { return pt.t < t; });
        if(it != path.begin())
          --it;

        run.resize(0);
        int run_key = -1;
        for(; it != path.end(); ++it)
        {
          const auto& pt = *it;
          // Fractional pitch: the row of the note plus its bend
          const double y = to_y(int(std::floor(pt.pitch)))
                           - (pt.pitch - std::floor(pt.pitch)) * row_h + row_h / 2;
          const QPointF here{to_x(pt.t), y};
          // A segment takes the expression of the point it leads to
          const int k = quantize(pt.timbre) * steps + quantize(pt.pressure);
          if(k != run_key && run.size() > 1)
          {
            p->setPen(pen_for(run_key / steps, run_key % steps));
            p->drawPolyline(run);
            const QPointF last = run.back();
            run.resize(0);
            run << last;
          }
          run_key = k;
          run << here;
        }
        // Still sounding: run to now
        if(!run.isEmpty() && m_open[chan][key % 128].active)
          run << QPointF{to_x(m_now), run.back().y()};
        if(run.size() > 1)
        {
          p->setPen(pen_for(run_key / steps, run_key % steps));
          p->drawPolyline(run);
        }
      }
      p->setRenderHint(QPainter::Antialiasing, false);
    }

    void paintList(QPainter* p) const
    {
      using namespace Ui::Monitor;
      const auto r = boundingRect();
      p->save();
      p->fillRect(r, background());
      QFont f = score::Skin::instance().MonoFontSmall;
      f.setPixelSize(10);
      p->setFont(f);
      const QFontMetricsF fm{f};
      const double lh = fm.height();
      const int lines = int((r.height() - header_height) / lh);
      const bool hide_clock = boolValue(m_clock_inlet, true);
      draw_header(p, r.width());
      // Newest at the top
      int row = 0;
      for(auto it = m_log.rbegin(); it != m_log.rend() && row < lines; ++it)
      {
        const auto& e = it->e;
        if(!shows(e))
          continue;
        if(hide_clock && (e.status == 0xF8 || e.status == 0xFE))
          continue;
        const double y = header_height + row * lh;
        const auto type = Ui::Monitor::midiMessageType(e.status, e.d1, e.d2);
        p->setPen(QColor(120, 120, 130));
        p->drawText(QRectF(4, y, 70, lh), Qt::AlignLeft | Qt::AlignVCenter, it->time);
        p->setPen(e.flags ? QColor(255, 120, 120) : kind_color(type).lighter(130));
        p->drawText(QRectF(74, y, r.width() - 160, lh), Qt::AlignLeft | Qt::AlignVCenter, it->text);
        p->setPen(QColor(110, 110, 120));
        p->drawText(QRectF(r.width() - 90, y, 86, lh), Qt::AlignRight | Qt::AlignVCenter, it->bytes);
        row++;
      }
      p->restore();
    }

    void paintLanes(QPainter* p) const
    {
      using namespace Ui::Monitor;
      const auto r = boundingRect();
      p->save();
      p->fillRect(r, background());
      draw_header(p, r.width());
      const int filter = channelFilter();
      std::vector<std::pair<int, const Lane*>> shown;
      for(const auto& [key, lane] : m_lanes)
        if(filter < 0 || key / 256 == filter)
          shown.emplace_back(key, &lane);
      if(shown.empty())
      {
        drawLabel(*p, r, Qt::AlignCenter, QStringLiteral("no controllers yet"));
        p->restore();
        return;
      }
      const double top = header_height, lh = (r.height() - top) / shown.size();
      const double x0 = left_gutter, w = r.width() - left_gutter - 2;
      const double win = window(), t_min = m_now - win;
      const auto to_x = TimeAxis(m_now, win, x0, w);
      for(std::size_t i = 0; i < shown.size(); i++)
      {
        const auto [key, lane] = shown[i];
        const QRectF row{x0, top + i * lh, w, lh};
        const int chan = key / 256, cc = key % 256;
        const QString name = cc == 128 ? QStringLiteral("bend") : cc == 129 ? QStringLiteral("press") : QStringLiteral("cc%1").arg(cc);
        p->setPen(gridColor());
        p->drawLine(row.bottomLeft(), row.bottomRight());
        drawLabel(*p, QRectF(0, row.top(), x0 - 2, std::min(lh, 22.)), Qt::AlignRight | Qt::AlignVCenter,
                  name + (lh >= 20 ? QStringLiteral("\nch%1").arg(chan + 1) : QString{}));
        if(cc == 128)
        {
          p->setPen(majorGridColor());
          p->drawLine(QPointF{row.left(), row.center().y()}, QPointF{row.right(), row.center().y()});
        }
        // Held values: steps, from the last one set before the window
        auto& line = m_polyline;
        line.resize(0);
        const auto& pts = lane->points;
        auto it = std::lower_bound(
            pts.begin(), pts.end(), t_min,
            [](const std::pair<double, float>& pt, double t) { return pt.first < t; });
        if(it != pts.begin())
          --it;
        for(; it != pts.end(); ++it)
        {
          const auto [t, v] = *it;
          const double y = row.bottom() - 1 - v * (row.height() - 2);
          if(!line.isEmpty())
            line << QPointF{to_x(t), line.back().y()};
          line << QPointF{to_x(t), y};
        }
        if(!line.isEmpty())
          line << QPointF{to_x(m_now), line.back().y()};
        p->setRenderHint(QPainter::Antialiasing, true);
        p->setPen(QPen(channelColor(chan), 1.3));
        p->drawPolyline(line);
        p->setRenderHint(QPainter::Antialiasing, false);
        if(!lane->points.empty() && lh >= 12)
          drawLabel(*p, row.adjusted(0, 0, -2, 0), Qt::AlignRight | Qt::AlignTop,
                    QString::number(cc == 128 ? int(lane->points.back().second * 16383) - 8192 : int(lane->points.back().second * 127)));
      }
      p->restore();
    }

    void paintActivity(QPainter* p) const
    {
      using namespace Ui::Monitor;
      const auto r = boundingRect();
      p->save();
      p->fillRect(r, background());
      draw_header(p, r.width());
      // Sixteen channel lights: bright when a message just came, fading over a second
      const double top = header_height + 2;
      const double led_w = (r.width() - 4) / 16.;
      const double led_h = std::min(18., led_w);
      for(int c = 0; c < 16; c++)
      {
        const double age = m_now - m_last_activity[c];
        const double glow = std::clamp(1. - age, 0., 1.);
        QColor col = kind_color(Ui::Monitor::midiMessageType(m_last_status[c], 0, 1));
        if((m_last_status[c] & 0xF0) == 0x90 || (m_last_status[c] & 0xF0) == 0x80)
          col = QColor(76, 200, 90);
        col = QColor::fromRgbF(
            0.15 + (col.redF() - 0.15) * glow, 0.15 + (col.greenF() - 0.15) * glow,
            0.17 + (col.blueF() - 0.17) * glow);
        const QRectF led{2 + c * led_w + 1, top, led_w - 2, led_h};
        p->fillRect(led, col);
        if(led_w >= 14)
        {
          p->setPen(glow > 0.5 ? Qt::black : textColor());
          p->setFont(smallFont());
          p->drawText(led, Qt::AlignCenter, QString::number(c + 1));
        }
      }
      // Keyboard of the held notes, all channels or the chosen one
      const QRectF kb{2, top + led_h + 6, r.width() - 4, r.height() - top - led_h - 8};
      if(kb.height() < 8)
      {
        p->restore();
        return;
      }
      const int filter = channelFilter();
      auto held = [&](int pitch) {
        for(int c = 0; c < 16; c++)
          if((filter < 0 || c == filter) && m_held_bits.test(c, pitch))
            return true;
        return false;
      };
      // White keys across, black keys over them
      static constexpr bool black[12]{0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};
      int whites = 0;
      for(int pitch = 21; pitch <= 108; pitch++)
        whites += !black[pitch % 12];
      const double ww = kb.width() / whites;
      double x = kb.left();
      std::vector<std::pair<double, int>> blacks;
      for(int pitch = 21; pitch <= 108; pitch++)
      {
        if(black[pitch % 12])
        {
          blacks.emplace_back(x, pitch);
          continue;
        }
        const QRectF key{x, kb.top(), ww - 1, kb.height()};
        p->fillRect(key, held(pitch) ? QColor(230, 190, 60) : QColor(225, 225, 225));
        if(pitch % 12 == 0 && ww >= 7)
        {
          p->setPen(QColor(90, 90, 90));
          p->setFont(smallFont());
          p->drawText(key.adjusted(0, 0, 0, -2), Qt::AlignHCenter | Qt::AlignBottom, Ui::Monitor::midiNoteName(pitch));
        }
        x += ww;
      }
      for(auto [bx, pitch] : blacks)
        p->fillRect(QRectF(bx - ww * 0.3, kb.top(), ww * 0.6, kb.height() * 0.6),
                    held(pitch) ? QColor(240, 160, 40) : QColor(30, 30, 30));
      p->restore();
    }

    QColor note_color(int vel, int chan) const noexcept
    {
      using namespace Ui::Monitor;
      const double t = std::clamp(vel / 127., 0., 1.);
      // The low end of the maps is too dark against the background
      auto from = [&](ColorMap m) { return QColor::fromRgb(colorMap(m)[int(40 + t * 215)]); };
      switch(enumValue(m_palette_inlet, NotePalette::Green, NotePalette::Channels))
      {
        case NotePalette::Inferno:
          return from(ColorMap::Inferno);
        case NotePalette::Viridis:
          return from(ColorMap::Viridis);
        case NotePalette::Magma:
          return from(ColorMap::Magma);
        case NotePalette::Turbo:
          return from(ColorMap::Turbo);
        case NotePalette::Channels:
          // Brighter with velocity
          return QColor::fromHsvF(std::fmod(chan * 0.618034, 1.), 0.65, 0.55 + 0.45 * t);
        default:
          return QColor(
              60 + int(80 * t), 150 + int(90 * t), 90 + int(40 * (1. - t)), 200 + int(55 * t));
      }
    }
  };

  struct Presenter : public Process::EffectLayerPresenter
  {
    using Process::EffectLayerPresenter::EffectLayerPresenter;
    void fillContextMenu(
        QMenu& menu, QPoint, QPointF, const Process::LayerContextMenuManager&) override
    {
      auto* copy = menu.addAction(tr("Copy MIDI log"));
      connect(copy, &QAction::triggered, this, [this] {
        qApp->clipboard()->setText(static_cast<Layer*>(this->m_view)->logText());
      });

      auto* clear = menu.addAction(tr("Clear MIDI log"));
      connect(clear, &QAction::triggered, this, [this] {
        static_cast<Layer*>(this->m_view)->reset();
      });
    }
  };
};
}
