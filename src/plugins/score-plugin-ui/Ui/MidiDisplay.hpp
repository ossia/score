#pragma once
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <Process/Dataflow/PortItem.hpp>
#include <Process/Process.hpp>

#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <Effect/EffectLayer.hpp>
#include <Effect/EffectLayout.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/model/Skin.hpp>

#include <ossia/network/value/value_conversion.hpp>

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QFontMetrics>
#include <QMenu>
#include <QPainter>

#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/midi.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
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
 * - The engine -> UI transport for control outputs is *lossy*: the UI polls
 *   `coarseUpdateTimer` (~2x the UI event rate, so roughly every 60-100ms) and
 *   only keeps the last value it finds in the queue. We therefore cannot push
 *   "the events of this tick" and expect the UI to see all of them. Instead the
 *   node keeps a small ring of the events of the last `retain_seconds` and
 *   pushes the *whole ring* every time; the layer deduplicates using the
 *   per-event sequence number. As long as the UI wakes up at least once per
 *   `retain_seconds`, no event can be missed.
 *
 * - A heartbeat push (every `heartbeat_seconds`) keeps the view scrolling when
 *   no MIDI is coming in.
 *
 * - Note pairing (held / stuck / retriggered / unmatched note-off) is done in
 *   the node, not in the layer, and shipped as a per-event flag. The layer only
 *   ever holds the last `retain_seconds` and is rebuilt from scratch when
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
struct Node
{
  halp_meta(name, "MIDI display")
  halp_meta(c_name, "MidiDisplay")
  halp_meta(category, "Monitoring")
  halp_meta(author, "ossia score")
  halp_meta(manual_url, "")
  halp_meta(description, "Visualize the MIDI messages going through a port")
  halp_meta(uuid, "a2b6c7f1-3d54-4e9a-9c21-8f0d5e73b104")
  halp_flag(fully_custom_item);

  static double recommended_height() { return 150.; }

  struct
  {
    halp::midi_bus<"in"> midi;
    halp::hslider_f32<"Window", halp::range{0.5f, 60.f, 8.f}> window;
  } inputs;

  struct
  {
    // [now, held, stuck, (seq, time, status, data1, data2, flags)...]
    struct : halp::val_port<"events", std::optional<std::vector<float>>>
    {
      enum widget
      {
        control
      };
    } events;
  } outputs;

  // How much history is packed in every push. Must comfortably exceed the UI
  // polling period, see the note above.
  static constexpr double retain_seconds = 1.0;
  static constexpr double heartbeat_seconds = 0.03;
  static constexpr int max_events = 512;
  static constexpr int fields_per_event = 6;
  static constexpr int header_fields = 3;
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

  double m_rate = 48000.;
  int64_t m_frames = 0;
  double m_last_push = -1e9;
  float m_seq = 0.f;
  std::deque<std::array<float, fields_per_event>> m_ring;
  // Pairing is done here rather than in the layer: this sees every message from
  // the first one on, while the layer only ever holds the last retain_seconds
  // and is rebuilt from scratch whenever playback restarts. Deriving "unmatched
  // note-off" or "note-on while held" from a truncated window invents both.
  std::array<std::array<held_note, 128>, 16> m_held_notes{};

  void prepare(halp::setup s) noexcept
  {
    m_rate = s.rate > 0 ? s.rate : 48000.;
    m_frames = 0;
    m_last_push = -1e9;
    m_seq = 0.f;
    m_ring.clear();
    m_held_notes = {};
  }

  using tick = halp::tick_musical;
  void operator()(halp::tick_musical t)
  {
    outputs.events.value.reset();

    const double t0 = m_frames / m_rate;
    const double t1 = (m_frames + t.frames) / m_rate;
    m_frames += t.frames;

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

      m_ring.push_back(
          {m_seq, float(ts), float(status), float(d1), float(d2),
           float(track(status, d1, d2, float(ts)))});
      m_seq += 1.f;
      got_message = true;
    }

    while(!m_ring.empty()
          && ((t1 - m_ring.front()[1]) > retain_seconds
              || std::ssize(m_ring) > max_events))
      m_ring.pop_front();

    if(got_message || (t1 - m_last_push) >= heartbeat_seconds)
    {
      int held = 0, stuck = 0;
      for(const auto& chan : m_held_notes)
      {
        for(const auto& note : chan)
        {
          if(!note.active)
            continue;
          held++;
          stuck += (t1 - note.on) > stuck_after_seconds;
        }
      }

      std::vector<float> payload;
      payload.reserve(header_fields + fields_per_event * m_ring.size());
      payload.push_back(float(t1));
      payload.push_back(float(held));
      payload.push_back(float(stuck));
      for(const auto& e : m_ring)
        payload.insert(payload.end(), e.begin(), e.end());

      outputs.events.value = std::move(payload);
      m_last_push = t1;
    }
  }

  //! Pairs a message against the running held-note state, returning what is
  //! anomalous about it.
  int track(uint8_t status, uint8_t d1, uint8_t d2, float t) noexcept
  {
    const int chan = status & 0x0F;
    switch(kind_of(status, d2))
    {
      case msg_kind::note_on: {
        auto& note = m_held_notes[chan][d1];
        const bool was_held = note.active;
        note = {true, t};
        return was_held ? flag_retrigger : flag_none;
      }
      case msg_kind::note_off: {
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

  enum class msg_kind
  {
    note_on,
    note_off,
    poly_pressure,
    control_change,
    program_change,
    aftertouch,
    pitch_bend,
    system
  };

  static msg_kind kind_of(int status, int data2) noexcept
  {
    switch(status & 0xF0)
    {
      case 0x80:
        return msg_kind::note_off;
      case 0x90:
        // Running-status note-off: note-on with a null velocity
        return data2 > 0 ? msg_kind::note_on : msg_kind::note_off;
      case 0xA0:
        return msg_kind::poly_pressure;
      case 0xB0:
        return msg_kind::control_change;
      case 0xC0:
        return msg_kind::program_change;
      case 0xD0:
        return msg_kind::aftertouch;
      case 0xE0:
        return msg_kind::pitch_bend;
      default:
        return msg_kind::system;
    }
  }

  static QString note_name(int pitch)
  {
    static const char* const names[12]
        = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    if(pitch < 0 || pitch > 127)
      return QStringLiteral("?");
    return QString::fromLatin1(names[pitch % 12]) + QString::number(pitch / 12 - 1);
  }

  struct Layer : public Process::EffectLayerView
  {
  public:
    static constexpr double left_gutter = 34.;
    static constexpr double header_height = 13.;
    static constexpr double system_lane_height = 9.;
    static constexpr int default_low_pitch = 48;
    static constexpr int default_high_pitch = 72;
    static constexpr int max_log = 256;

    struct Event
    {
      float seq{};
      float t{};
      uint8_t status{};
      uint8_t d1{};
      uint8_t d2{};
      uint8_t flags{};
    };

    struct Note
    {
      float on{};
      float off{};
      uint8_t chan{};
      uint8_t pitch{};
      uint8_t vel{};
      bool retriggered{}; // note-on while already held: the note-off went missing
    };

    struct OpenNote
    {
      bool active{};
      float on{};
      uint8_t vel{};
    };

    struct OrphanOff
    {
      float t{};
      uint8_t chan{};
      uint8_t pitch{};
    };

    Scenario::IntervalModel* m_interval{};
    Process::ControlInlet* m_window_inlet{};

    std::deque<Event> m_log;
    std::deque<Note> m_notes;
    std::deque<OrphanOff> m_orphans;
    std::array<std::array<OpenNote, 128>, 16> m_open{};

    float m_now = 0.f;
    float m_last_seq = -1.f;
    // Held / stuck counts come from the node: it has seen the whole stream.
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

      const Process::PortFactoryList& portFactory
          = doc.app.interfaces<Process::PortFactoryList>();

      // FullyCustomItem means the node item does not lay out any port: we have
      // to instantiate the ones we want reachable ourselves.
      auto& midi_inlet = *process.inlets()[0];
      if(auto* fact = portFactory.get(midi_inlet.concreteKey()))
        if(auto* item = fact->makePortItem(midi_inlet, doc, this, this))
          item->setPos(0, 5);

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

      auto* events_outlet
          = static_cast<Process::ControlOutlet*>(process.outlets().back());
      connect(
          events_outlet, &Process::ControlOutlet::valueChanged, this,
          [this](const ossia::value& v) { on_events(v); });
    }

    double window() const noexcept
    {
      if(!m_window_inlet)
        return 8.;
      const double w = ossia::convert<float>(m_window_inlet->value());
      return std::isfinite(w) && w > 0.05 ? w : 8.;
    }

    void reset()
    {
      m_log.clear();
      m_notes.clear();
      m_orphans.clear();
      m_open = {};
      m_now = 0.f;
      m_last_seq = -1.f;
      m_held = 0;
      m_stuck = 0;
      update();
    }

    void on_events(const ossia::value& v)
    {
      auto* list = v.target<std::vector<ossia::value>>();
      if(!list || list->empty())
        return;

      const int N = std::ssize(*list);
      if(N < header_fields || (N - header_fields) % fields_per_event != 0)
        return;

      const float now = ossia::convert<float>((*list)[0]);

      // The node restarted (new playback): our timeline is stale.
      if(now < m_now)
        reset();
      m_now = now;
      m_held = ossia::convert<int>((*list)[1]);
      m_stuck = ossia::convert<int>((*list)[2]);

      for(int i = header_fields; i + fields_per_event <= N; i += fields_per_event)
      {
        const float seq = ossia::convert<float>((*list)[i]);
        if(seq <= m_last_seq)
          continue;
        m_last_seq = seq;

        Event e;
        e.seq = seq;
        e.t = ossia::convert<float>((*list)[i + 1]);
        e.status = uint8_t(std::clamp(ossia::convert<float>((*list)[i + 2]), 0.f, 255.f));
        e.d1 = uint8_t(std::clamp(ossia::convert<float>((*list)[i + 3]), 0.f, 127.f));
        e.d2 = uint8_t(std::clamp(ossia::convert<float>((*list)[i + 4]), 0.f, 127.f));
        e.flags = uint8_t(std::clamp(ossia::convert<float>((*list)[i + 5]), 0.f, 255.f));

        apply(e);

        m_log.push_back(e);
        if(std::ssize(m_log) > max_log)
          m_log.pop_front();
      }

      trim();
      update();
    }

    // The flags are computed by the node against the full stream. This only
    // turns them into geometry: an unmatched message here means our window
    // simply does not reach far enough back, which is not an anomaly.
    void apply(const Event& e)
    {
      const int chan = e.status & 0x0F;
      switch(kind_of(e.status, e.d2))
      {
        case msg_kind::note_on: {
          auto& open = m_open[chan][e.d1];
          if(open.active)
          {
            m_notes.push_back(
                {open.on, e.t, uint8_t(chan), e.d1, open.vel,
                 bool(e.flags & flag_retrigger)});
          }
          open = {true, e.t, e.d2};
          break;
        }
        case msg_kind::note_off: {
          if(e.flags & flag_orphan)
          {
            m_orphans.push_back({e.t, uint8_t(chan), e.d1});
            break;
          }

          auto& open = m_open[chan][e.d1];
          if(open.active)
          {
            m_notes.push_back({open.on, e.t, uint8_t(chan), e.d1, open.vel, false});
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
      const float horizon = m_now - 61.f;
      while(!m_notes.empty() && m_notes.front().off < horizon)
        m_notes.pop_front();
      while(!m_orphans.empty() && m_orphans.front().t < horizon)
        m_orphans.pop_front();
    }

    int held_count() const noexcept { return m_held; }
    int stuck_count() const noexcept { return m_stuck; }

    QString logText() const
    {
      QString out;
      for(const auto& e : m_log)
      {
        out += QString::asprintf("%9.4f  ", double(e.t));
        out += describe(e);
        out += QLatin1Char('\n');
      }
      return out;
    }

    static QString describe(const Event& e)
    {
      const int chan = (e.status & 0x0F) + 1;
      switch(kind_of(e.status, e.d2))
      {
        case msg_kind::note_on:
          return QStringLiteral("ch%1  Note On   %2 (%3) vel %4")
              .arg(chan, 2)
              .arg(int(e.d1), 3)
              .arg(note_name(e.d1))
              .arg(int(e.d2));
        case msg_kind::note_off:
          return QStringLiteral("ch%1  Note Off  %2 (%3) vel %4")
              .arg(chan, 2)
              .arg(int(e.d1), 3)
              .arg(note_name(e.d1))
              .arg(int(e.d2));
        case msg_kind::poly_pressure:
          return QStringLiteral("ch%1  Poly Aft. %2 -> %3")
              .arg(chan, 2)
              .arg(int(e.d1), 3)
              .arg(int(e.d2));
        case msg_kind::control_change:
          return QStringLiteral("ch%1  CC        %2 -> %3")
              .arg(chan, 2)
              .arg(int(e.d1), 3)
              .arg(int(e.d2));
        case msg_kind::program_change:
          return QStringLiteral("ch%1  Program   %2").arg(chan, 2).arg(int(e.d1), 3);
        case msg_kind::aftertouch:
          return QStringLiteral("ch%1  Aftertouch %2").arg(chan, 2).arg(int(e.d1), 3);
        case msg_kind::pitch_bend:
          return QStringLiteral("ch%1  Pitch Bend %2")
              .arg(chan, 2)
              .arg(int(e.d1) + (int(e.d2) << 7) - 8192);
        default:
          return QStringLiteral("     System    0x%1 %2 %3")
              .arg(int(e.status), 2, 16, QLatin1Char('0'))
              .arg(int(e.d1), 3)
              .arg(int(e.d2), 3);
      }
    }

    static QColor kind_color(msg_kind k) noexcept
    {
      switch(k)
      {
        case msg_kind::control_change:
          return QColor(90, 160, 230);
        case msg_kind::pitch_bend:
          return QColor(180, 130, 230);
        case msg_kind::program_change:
          return QColor(230, 190, 90);
        case msg_kind::poly_pressure:
        case msg_kind::aftertouch:
          return QColor(120, 200, 190);
        default:
          return QColor(150, 150, 150);
      }
    }

    void paint_impl(QPainter* p) const override
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

      const auto to_x = [&](double t) {
        return roll_x + std::clamp((t - t_min) / win, 0., 1.) * roll_w;
      };

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
      QFont f = p->font();
      f.setPixelSize(8);
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
            note_name(pitch));
      }
    }

    void draw_notes(
        QPainter* p, double row_h, double t_min, auto to_x, auto to_y) const
    {
      p->setPen(Qt::NoPen);
      const double bar_h = std::max(1., row_h - 1.);

      for(const auto& n : m_notes)
      {
        if(n.off < t_min)
          continue;
        const double x0 = to_x(n.on);
        const double x1 = std::max(to_x(n.off), x0 + 1.);
        QColor col = n.retriggered ? QColor(240, 160, 40) : velocity_color(n.vel);
        p->fillRect(QRectF(x0, to_y(n.pitch), x1 - x0, bar_h), col);
      }

      // Notes still held: they run all the way to "now". If they have been held
      // for longer than the threshold, this is the missing-note-off symptom.
      for(int chan = 0; chan < 16; chan++)
      {
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

      for(const auto& e : m_log)
      {
        if(e.t < t_min)
          continue;
        const auto k = kind_of(e.status, e.d2);
        if(k == msg_kind::note_on || k == msg_kind::note_off)
          continue;
        p->setPen(kind_color(k));
        const double x = to_x(e.t);
        p->drawLine(QPointF(x, y + 1.), QPointF(x, y + system_lane_height - 1.));
      }
    }

    void draw_header(QPainter* p, double W) const
    {
      QFont f = p->font();
      f.setPixelSize(9);
      p->setFont(f);

      const QFontMetricsF fm{f};
      const auto band = [&](double x, double w) { return QRectF(x, 0, w, header_height); };
      constexpr double gap = 8.;

      double x = 2.;

      const auto held = QStringLiteral("held %1").arg(held_count());
      p->setPen(QColor(160, 160, 168));
      p->drawText(band(x, fm.horizontalAdvance(held)), Qt::AlignVCenter, held);
      x += fm.horizontalAdvance(held) + gap;

      if(const int stuck = stuck_count(); stuck > 0)
      {
        const auto txt = QStringLiteral("STUCK %1").arg(stuck);
        p->setPen(QColor(255, 90, 90));
        p->drawText(band(x, fm.horizontalAdvance(txt)), Qt::AlignVCenter, txt);
        x += fm.horizontalAdvance(txt) + gap;
      }

      if(!m_log.empty())
      {
        const auto txt = describe(m_log.back());
        const double avail = W - 2. - x;
        if(avail > fm.horizontalAdvance(QStringLiteral("ch16  Note Off  ")))
        {
          p->setPen(QColor(130, 130, 140));
          p->drawText(
              band(x, avail), Qt::AlignRight | Qt::AlignVCenter,
              fm.elidedText(txt, Qt::ElideRight, avail));
        }
      }
    }

    static QColor velocity_color(int vel) noexcept
    {
      const double t = std::clamp(vel / 127., 0., 1.);
      return QColor(
          60 + int(80 * t), 150 + int(90 * t), 90 + int(40 * (1. - t)), 200 + int(55 * t));
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
