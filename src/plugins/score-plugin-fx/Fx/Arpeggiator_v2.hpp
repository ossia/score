#pragma once
#include <Advanced/MidiScaler/MidiCommon/EventQueue.hpp>
#include <Advanced/MidiScaler/MidiCommon/Messages.hpp>
#include <Advanced/MidiScaler/MidiCommon/Rhythm.hpp>
#include <Fx/Arpeggiator.hpp>
#include <halp/layout.hpp>

#include <cmath>

namespace Nodes::Arpeggiator::v2
{
//! The first six keep the names and indices of the old arpeggiator's.
struct Arpeggios
{
  halp_meta(name, "Arpeggios");
  halp_meta(
      description,
      "Order of the steps. F->B and B->F play the top and bottom twice, Up/Down "
      "and Down/Up once. Low Up and High Up alternate the lowest or highest note "
      "with the others. Chord plays every note at each step: with Pressure, it "
      "is a note repeat.");
  enum widget
  {
    combobox
  };

  struct range
  {
    halp::combo_pair<int> values[13]{
        {"Forward", 0},   {"Backward", 1},  {"F->B", 2},   {"B->F", 3},
        {"Chord", 4},     {"Random", 5},    {"Up/Down", 6}, {"Down/Up", 7},
        {"Converge", 8},  {"Diverge", 9},   {"Low Up", 10}, {"High Up", 11},
        {"As played", 12}};
    int init{0};
  };

  int value{};
};

enum RetriggerMode
{
  NoRetrigger,
  RetriggerOnNote,
  RetriggerOnBeat
};
struct Retrigger
{
  halp_meta(name, "Retrigger");
  halp_meta(
      description, "Restart the pattern from its first step on each new key, or "
                   "on each beat");
  enum widget
  {
    combobox
  };
  struct range
  {
    halp::combo_pair<int> values[3]{{"Off", 0}, {"Note", 1}, {"Beat", 2}};
    int init{0};
  };
  int value{};
};

enum LatchMode
{
  NoLatch,
  LatchToggle,
  LatchMomentary
};
struct Latch
{
  halp_meta(name, "Latch");
  halp_meta(
      description, "Toggle: released notes keep playing until a new chord. "
                   "Momentary: released notes keep playing while Hold is on.");
  enum widget
  {
    combobox
  };
  struct range
  {
    halp::combo_pair<int> values[3]{{"Off", 0}, {"Toggle", 1}, {"Momentary", 2}};
    int init{0};
  };
  int value{};
};

enum PressureMode
{
  NoPressure,
  ChannelPressure,
  PolyPressure,
  AnyPressure
};
struct Pressure
{
  halp_meta(name, "Pressure");
  halp_meta(
      description, "Aftertouch sets the velocity of the following steps: channel "
                   "pressure for every note, poly pressure for its own key");
  enum widget
  {
    combobox
  };
  struct range
  {
    halp::combo_pair<int> values[4]{
        {"Off", 0}, {"Channel", 1}, {"Poly", 2}, {"Both", 3}};
    int init{0};
  };
  int value{};
};

struct Node
{
  halp_meta(name, "Arpeggiator")
  halp_meta(c_name, "Arpeggiator")
  halp_meta(category, "Midi")
  halp_meta(author, "ossia score")
  halp_meta(
      manual_url,
      "https://ossia.io/score-docs/processes/midi-utilities.html#arpeggiator")
  halp_meta(
      description,
      "Arpeggiator. For a note repeat, choose the Chord pattern, a Rate and a "
      "Pressure mode: the held notes repeat at the rate, at the velocity of the "
      "key pressure.")
  halp_meta(uuid, "4df48c1c-f5f2-4ab1-9f37-3b0adba139ae")

  using byte = unsigned char;

  struct ins
  {
    halp::midi_bus<"in", libremidi::message> midi;
    Arpeggios arpeggios;
    halp::hslider_i32<"Octave", halp::irange{1, 7, 1}> octave;
    OctaveMode octave_mode;
    halp::hslider_i32<"Repeat", halp::irange{1, 8, 1}> repeat;
    //! Between two steps: seconds, or a note value, on the bars' grid.
    halp::time_chooser<"Rate", halp::range{0.01, 4., 0.125}> rate;

    struct : halp::hslider_i32<"Gate", halp::irange{1, 200, 100}>
    {
      halp_meta(
          description, "Length of a note in percent of a step. 100 lasts until the "
                       "next step, above overlaps it.")
    } gate;
    struct : halp::hslider_i32<"Swing", halp::irange{0, 100, 0}>
    {
      halp_meta(
          description, "Delays every second step: 50 puts it on the triplet, 100 at "
                       "5/6 of the pair")
    } swing;
    struct : halp::spinbox_i32<"Accent every", halp::irange{0, 16, 0}>
    {
      halp_meta(description, "Every Nth step, from the first, plays at the accent velocity. 0: no accent.")
    } accent_every;
    struct : halp::hslider_i32<"Accent vel.", halp::irange{1, 127, 127}>
    {
      halp_meta(description, "Velocity of the accented steps")
    } accent_velocity;
    struct : halp::hslider_i32<"Vel. decay", halp::irange{0, 100, 0}>
    {
      halp_meta(
          description, "At each step since the chord started, the velocity moves "
                       "this percentage of the way to the decay target")
    } decay;
    struct : halp::hslider_i32<"Decay target", halp::irange{1, 127, 1}>
    {
      halp_meta(description, "Velocity the decay tends to")
    } decay_target;
    Retrigger retrigger;
    Latch latch;
    struct : halp::toggle<"Latch hold">
    {
      halp_meta(description, "With the Momentary latch: released notes keep playing while on")
    } hold;
    Pressure pressure;
    struct : halp::spinbox_i32<"Seed", halp::irange{0, 10000, 0}>
    {
      halp_meta(description, "Random pattern: 0 runs freely, any other value repeats the same sequence after each stop")
    } seed;
  } inputs;
  struct
  {
    halp::midi_out_bus<"out", libremidi::message> midi;
  } outputs;

  struct ui
  {
    halp_meta(name, "Arpeggiator")
    halp_meta(layout, halp::layouts::hbox)
    halp_meta(background, halp::colors::background_dark)

    struct
    {
      halp_meta(name, "Pattern")
      halp_meta(layout, halp::layouts::section)
      halp_meta(background, halp::colors::background_mid)
      halp::item<&ins::arpeggios> arpeggios;
      halp::item<&ins::octave> octave;
      halp::item<&ins::octave_mode> octave_mode;
      halp::item<&ins::repeat> repeat;
      halp::item<&ins::rate> rate;
    } pattern;

    struct
    {
      halp_meta(name, "Timing")
      halp_meta(layout, halp::layouts::section)
      halp_meta(background, halp::colors::background_mid)
      halp::item<&ins::gate> gate;
      halp::item<&ins::swing> swing;
      halp::item<&ins::retrigger> retrigger;
    } timing;

    struct
    {
      halp_meta(name, "Velocity")
      halp_meta(layout, halp::layouts::section)
      halp_meta(background, halp::colors::background_mid)
      halp::item<&ins::accent_every> accent_every;
      halp::item<&ins::accent_velocity> accent_velocity;
      halp::item<&ins::decay> decay;
      halp::item<&ins::decay_target> decay_target;
      halp::item<&ins::pressure> pressure;
    } velocity;

    struct
    {
      halp_meta(name, "Latch")
      halp_meta(layout, halp::layouts::section)
      halp_meta(background, halp::colors::background_mid)
      halp::item<&ins::latch> latch;
      struct
      {
        halp_meta(layout, halp::layouts::vbox)
        static constexpr auto condition()
        {
          return halp::enabled_when<&ins::latch, int(LatchMomentary)>{};
        }
        halp::item<&ins::hold> hold;
      } hold;
      halp::item<&ins::seed> seed;
    } latch;
  };

  //! `notes` is what plays: the held keys, and with a latch, the released ones.
  Engine engine;

  Node()
  {
    std::random_device d{};
    m_free_seed = (uint64_t(d()) << 32) ^ d();
  }

  halp::setup setup;
  void prepare(halp::setup s) noexcept { setup = s; }

  void stop() noexcept { m_stop = true; }

  //! The grid's rate for the step (1 a whole note, 4 a quarter...): a
  //! synced step is a note value; a free one, its length at the tempo.
  double grid_rate(const halp::tick_musical& tk) const noexcept
  {
    const double secs = inputs.rate.value;
    if(!(secs > 0.) || !(tk.tempo > 0.))
      return 0.;
    return 240. / (secs * tk.tempo);
  }

  using tick = halp::tick_musical;
  void operator()(const halp::tick_musical& tk) noexcept
  {
    const int frames = std::max(0, tk.frames);
    m_frames = frames;

    bool dirty = false;
    if(m_stop)
    {
      m_stop = false;
      reset();
      dirty = true;
    }

    if(inputs.seed.value != m_seed)
    {
      m_seed = inputs.seed.value;
      reseed();
    }

    for(const auto& m : inputs.midi)
      dirty |= handle(m, frames > 0 ? std::clamp<int64_t>(m.timestamp, 0, frames - 1) : 0);

    // Leaving a latch: what is not held any more stops
    const bool sustaining = this->sustaining();
    if(m_sustaining && !sustaining)
    {
      for(int p = 0; p < 128; p++)
        if(m_held[p] == 0 && engine.notes.find(byte(p)) != engine.notes.end())
        {
          forget(p);
          dirty = true;
        }
    }
    m_sustaining = sustaining;

    const int mode = std::clamp(inputs.arpeggios.value, 0, 12);
    const int octave = inputs.octave;
    const int octave_mode = inputs.octave_mode.value;
    const int repeat = inputs.repeat;
    if(dirty || octave != engine.previous_octave
       || octave_mode != engine.previous_octave_mode
       || repeat != engine.previous_repeat || mode != engine.previous_arpeggio)
    {
      engine.previous_octave = octave;
      engine.previous_octave_mode = octave_mode;
      engine.previous_repeat = repeat;
      engine.previous_arpeggio = mode;
      engine.update();
    }

    if(engine.arpeggio.empty())
    {
      end_all(0);
      m_pending.valid = false;
      m_step = 0;
      m_channel_pressure = -1;
      finish(frames);
      return;
    }

    if(engine.index >= engine.arpeggio.size())
      engine.index = 0;

    const double rate = grid_rate(tk);
    const double dq = tk.end_position_in_quarters - tk.start_position_in_quarters;
    const bool musical = dq > 0. && frames > 0;
    const double sr = setup.rate > 0. ? setup.rate : 48000.;
    const double frames_per_quarter
        = musical ? frames / dq : (tk.tempo > 0. ? 60. / tk.tempo * sr : 0.5 * sr);
    m_step_frames = rate > 0. ? frames_per_quarter * 4. / rate : double(std::max(1, frames));

    if(frames > 0)
    {
      for(auto [date, bar] : tk.get_quantification_date_with_bars(rate))
      {
        if(date >= frames)
          break;
        const int64_t at = m_now + date;
        if(m_pending.valid && m_pending.at <= at)
          fire_pending();

        // Where the step falls on the grid: for the swing pairs and the beats
        int64_t grid_index = m_free_grid++;
        bool on_beat = false;
        if(musical && rate > 0.)
        {
          const double q = tk.start_position_in_quarters + date * dq / frames;
          grid_index = std::llround(q * rate / 4.);
          on_beat = std::abs(q - std::round(q)) <= 0.5 * dq / frames + 1e-9;
        }
        if(inputs.retrigger.value == RetriggerOnBeat && on_beat)
          restart();

        std::size_t play_index;
        if(mode == 5) // Random
          play_index = m_rng() % engine.arpeggio.size();
        else
        {
          play_index = engine.index;
          engine.index = (engine.index + 1) % engine.arpeggio.size();
        }

        // The chosen notes and their velocities, kept until the swung date
        Step s;
        s.valid = true;
        const int64_t delay = std::llround(
            mtk::swing_delay(grid_index, inputs.swing.value / 100.) * m_step_frames);
        s.at = at + std::max<int64_t>(0, delay);
        for(auto [pitch, vel] : engine.arpeggio[play_index])
        {
          if(s.count == 128)
            break;
          s.notes[s.count++] = {pitch, byte(velocity(vel, m_step))};
        }
        m_step++;

        if(m_pending.valid)
        {
          // Swung past this step (the rate changed): it plays now
          m_pending.at = std::min(m_pending.at, at);
          fire_pending();
        }
        m_pending = s;
        if(s.at < m_now + frames)
          fire_pending();
      }
      if(m_pending.valid && m_pending.at < m_now + frames)
        fire_pending();
    }
    finish(frames);
  }

private:
  struct Step
  {
    bool valid{};
    int64_t at{};
    int count{};
    std::array<std::pair<byte, byte>, 128> notes{};
  };

  bool sustaining() const noexcept
  {
    return inputs.latch.value == LatchToggle
           || (inputs.latch.value == LatchMomentary && inputs.hold.value);
  }

  void reseed() noexcept
  {
    if(m_seed != 0)
      m_rng.seed(uint64_t(m_seed), 0x5851f42d4c957f2dULL);
    else
      m_rng.seed(m_free_seed, 0x14057b7ef767814fULL);
  }

  void restart() noexcept
  {
    engine.index = 0;
    m_step = 0;
  }

  //! Velocity of a step's note: the key's (or the pressure's), moved towards
  //! the decay target, replaced on accented steps.
  int velocity(int key_velocity, int64_t step) const noexcept
  {
    double v = m_channel_pressure >= 0 ? m_channel_pressure : key_velocity;
    if(inputs.decay.value > 0)
    {
      const double target = inputs.decay_target.value;
      v = target
          + (v - target) * std::pow(1. - inputs.decay.value / 100., double(step));
    }
    if(inputs.accent_every.value > 0 && step % inputs.accent_every.value == 0)
      v = inputs.accent_velocity.value;
    return std::clamp(int(std::lround(v)), 1, 127);
  }

  void add(int p, int v) noexcept
  {
    auto [it, inserted] = engine.notes.insert({byte(p), byte(v)});
    if(!inserted)
      it->second = byte(v);
    else if(engine.order_count < 128)
      engine.order[engine.order_count++] = byte(p);
  }

  void forget(int p) noexcept
  {
    engine.notes.erase(byte(p));
    int w = 0;
    for(int r = 0; r < engine.order_count; r++)
      if(engine.order[r] != p)
        engine.order[w++] = engine.order[r];
    engine.order_count = w;
  }

  void forget_all() noexcept
  {
    engine.notes.clear();
    engine.order_count = 0;
    m_held.fill(0);
    m_held_count = 0;
  }

  //! Returns whether the notes to play changed.
  bool handle(const libremidi::message& m, int64_t ts) noexcept
  {
    switch(mtk::classify(m))
    {
      case mtk::midi_kind::note_on: {
        const int p = m.bytes[1], v = m.bytes[2];
        if(m_held_count == 0 && inputs.latch.value == LatchToggle)
          forget_all();
        if(engine.notes.empty())
        {
          m_step = 0;
          m_channel_pressure = -1;
        }
        if(m_held[p] == 0)
        {
          m_held_count++;
          if(inputs.retrigger.value == RetriggerOnNote)
            restart();
        }
        m_held[p] = byte(v);
        add(p, v);
        return true;
      }
      case mtk::midi_kind::note_off: {
        const int p = m.bytes[1];
        if(m_held[p] != 0)
        {
          m_held[p] = 0;
          m_held_count--;
        }
        if(!sustaining())
          forget(p);
        return true;
      }
      case mtk::midi_kind::channel_pressure:
        if(inputs.pressure.value == ChannelPressure || inputs.pressure.value == AnyPressure)
        {
          m_channel_pressure = std::max(1, int(m.bytes[1]));
          return false;
        }
        break;
      case mtk::midi_kind::poly_pressure:
        if(inputs.pressure.value == PolyPressure || inputs.pressure.value == AnyPressure)
        {
          if(auto it = engine.notes.find(m.bytes[1]); it != engine.notes.end())
          {
            // The most recent kind of pressure wins: per-key values take over
            // again from an earlier channel pressure.
            m_channel_pressure = -1;
            it->second = byte(std::max(1, int(m.bytes[2])));
            return true;
          }
          return false;
        }
        break;
      case mtk::midi_kind::control_change:
        if(mtk::is_all_notes_off(m))
        {
          end_all(ts);
          m_pending.valid = false;
          forget_all();
          restart();
          m_channel_pressure = -1;
          auto out = m;
          out.timestamp = ts;
          outputs.midi.push_back(out);
          return true;
        }
        break;
      default:
        break;
    }
    auto out = m;
    out.timestamp = ts;
    outputs.midi.push_back(out);
    return false;
  }

  void send_off(int p, int64_t ts) noexcept
  {
    outputs.midi.note_off(1, byte(p), 0).timestamp = ts;
  }

  //! Every sounding note ends at `ts`; nothing stays scheduled.
  void end_all(int64_t ts) noexcept
  {
    m_floor = std::max(m_floor, ts);
    for(int p = 0; p < 128; p++)
      if(m_sounding[p])
      {
        m_sounding[p] = 0;
        send_off(p, ts);
      }
    m_offs.clear();
  }

  //! Sends the scheduled note-offs dated before `ts` in the tick.
  void release_until(int64_t ts) noexcept
  {
    m_offs.release(m_now, int(ts), [this](libremidi::message&& msg) {
      const int p = msg.bytes[1];
      if(m_sounding[p])
      {
        m_sounding[p] = 0;
        outputs.midi.push_back(std::move(msg));
      }
    });
  }

  void fire_pending() noexcept
  {
    Step& s = m_pending;
    s.valid = false;
    // Never before what was already sent in this tick: the output stays in
    // time order for each pitch
    const int64_t ts = std::clamp<int64_t>(
        std::max(s.at - m_now, m_floor), 0, std::max(0, m_frames - 1));
    m_floor = ts;
    release_until(ts);

    const int gate = inputs.gate.value;
    if(gate <= 100)
    {
      // The previous step ends where this one starts, at the latest
      end_all(ts);
    }
    else
    {
      // Overlapping steps: a pitch played again ends first
      for(int i = 0; i < s.count; i++)
      {
        const int p = s.notes[i].first;
        if(m_sounding[p])
        {
          m_sounding[p] = 0;
          send_off(p, ts);
          m_offs.remove_if([p](const auto& e) { return e.msg.bytes[1] == p; });
        }
      }
    }

    const int64_t length
        = std::max<int64_t>(1, std::llround(m_step_frames * gate / 100.));
    for(int i = 0; i < s.count; i++)
    {
      const auto [p, v] = s.notes[i];
      if(m_sounding[p])
        continue; // twice in the same step
      m_sounding[p] = 1;
      outputs.midi.note_on(1, p, v).timestamp = ts;
      // At 100 the note lasts until the next step, wherever it falls
      if(gate != 100)
        m_offs.schedule(
            m_now + ts + length, libremidi::channel_events::note_off(1, p, 0));
    }
  }

  void reset() noexcept
  {
    end_all(0);
    m_pending.valid = false;
    forget_all();
    restart();
    m_channel_pressure = -1;
    m_sustaining = false;
    reseed();
  }

  //! The rest of the scheduled note-offs, the output in time order, the clock.
  void finish(int frames) noexcept
  {
    release_until(frames);
    // Stable insertion sort: same-frame messages keep the order they were
    // made in, and nothing is allocated.
    auto& msgs = outputs.midi.midi_messages;
    for(std::size_t i = 1; i < msgs.size(); i++)
    {
      if(!(msgs[i].timestamp < msgs[i - 1].timestamp))
        continue;
      auto m = std::move(msgs[i]);
      std::size_t j = i;
      for(; j > 0 && m.timestamp < msgs[j - 1].timestamp; j--)
        msgs[j] = std::move(msgs[j - 1]);
      msgs[j] = std::move(m);
    }
    m_now += frames;
    m_floor = 0;
  }

  mtk::event_queue<512> m_offs;
  Step m_pending;
  std::array<byte, 128> m_held{};
  int m_held_count{};
  std::array<byte, 128> m_sounding{};
  int64_t m_now{};
  int64_t m_floor{}; //!< earliest frame of the tick a step may still use
  int64_t m_step{};
  int64_t m_free_grid{};
  double m_step_frames{1.};
  int m_frames{};
  int m_channel_pressure{-1};
  int m_seed{-1};
  uint64_t m_free_seed{};
  rnd::pcg m_rng;
  bool m_sustaining{};
  bool m_stop{};
};
}
