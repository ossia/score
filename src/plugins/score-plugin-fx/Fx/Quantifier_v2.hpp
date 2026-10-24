#pragma once
#include <Advanced/MidiScaler/MidiCommon/Messages.hpp>
#include <Fx/Quantifier.hpp>
#include <halp/layout.hpp>

#include <algorithm>
#include <cmath>

namespace Nodes::Quantifier::v2
{
//! How long a quantized note lasts.
enum NoteLength
{
  //! Until its note-off comes; one released before its start keeps the length
  //! it was played with.
  UntilNoteOff,
  //! Duration, whatever the note-off does.
  FixedDuration,
  //! Until the next point of the Duration grid, whatever the note-off does.
  EndOnGrid
};

struct Node
{
  halp_meta(name, "Midi quantify")
  halp_meta(c_name, "Quantifier")
  halp_meta(category, "Midi/Timing")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/midi-utilities.html#quantifier")
  halp_meta(author, "ossia score")
  halp_meta(description, "Quantifies a MIDI input")
  halp_meta(uuid, "a0cb1144-05a8-4f9b-b22c-81cd964314fb")
  halp_flag(process_exec);

  struct ins
  {
    halp::midi_bus<"in", libremidi::message> midi;
    //! Notes start on this grid; 0 lets them through as they come.
    halp::time_chooser<"Grid", halp::range{0., 4., 0.125}> grid;
    //! 1: a note always waits for the next grid point. Lower: a note played
    //! just after a grid point -- by up to (1 - tightness) of half a step --
    //! counts as on time, and starts at once.
    struct : halp::knob_f32<"Tightness", halp::range{0.f, 1.f, 0.8f}>
    {
      halp_meta(
          description, "1: a note always waits for the next grid point. Lower: a "
                       "note just after a grid point starts at once.")
    } tightness;
    halp::enum_t<NoteLength, "Length"> length;
    //! The length of a note, or the grid its end falls on.
    halp::time_chooser<"Duration", halp::range{0., 8., 0.25}> duration;

    struct : halp::knob_f32<"Gate", halp::range{0., 2., 1.}>
    {
      halp_meta(description, "Scales the length of the notes: 1 keeps it, 2 doubles it")
    } gate;
    struct : halp::toggle<"Trigger">
    {
      halp_meta(
          description,
          "A note plays when its key is released, quantized like a note-on. With "
          "Until note-off, it lasts as long as the key was held.")
    } trigger;
    struct : halp::knob_f32<"Decay", halp::range{0., 1., 0.}>
    {
      halp_meta(
          description,
          "A key pressed again while its last note still sounds or waits, and with "
          "Trigger every release, plays at the velocity of the key's previous note "
          "lowered by this proportion")
    } decay;
    struct
    {
      halp_meta(name, "Release vel.");
      halp_meta(
          description, "Velocity of the note-offs: the fixed value, or the one of "
                       "the key's release");
      enum widget
      {
        combobox
      };
      struct range
      {
        halp::combo_pair<int> values[2]{{"Fixed", 0}, {"Input", 1}};
        int init{0};
      };
      int value{};
    } release_mode;
    struct : halp::spinbox_i32<"Release value", halp::irange{0, 127, 0}>
    {
      halp_meta(description, "Fixed velocity of the note-offs")
    } release_velocity;
  } inputs;
  struct
  {
    midi_out midi;
  } outputs;

  struct ui
  {
    halp_meta(name, "Midi quantify")
    halp_meta(layout, halp::layouts::hbox)
    halp_meta(background, halp::colors::background_dark)

    struct
    {
      halp_meta(name, "Grid")
      halp_meta(layout, halp::layouts::section)
      halp_meta(background, halp::colors::background_mid)
      halp::item<&ins::grid> grid;
      halp::item<&ins::tightness> tightness;
    } grid;

    struct
    {
      halp_meta(name, "Length")
      halp_meta(layout, halp::layouts::section)
      halp_meta(background, halp::colors::background_mid)
      halp::item<&ins::length> length;
      struct
      {
        halp_meta(layout, halp::layouts::hbox)
        struct
        {
          halp_meta(layout, halp::layouts::vbox)
          static constexpr auto condition()
          {
            return halp::enabled_when<&ins::length, FixedDuration, EndOnGrid>{};
          }
          halp::item<&ins::duration> duration;
        } duration;
        halp::item<&ins::gate> gate;
      } knobs;
    } length;

    struct
    {
      halp_meta(name, "Release")
      halp_meta(layout, halp::layouts::section)
      halp_meta(background, halp::colors::background_mid)
      halp::item<&ins::trigger> trigger;
      halp::item<&ins::decay> decay;
      halp::item<&ins::release_mode> release_mode;
      struct
      {
        halp_meta(layout, halp::layouts::vbox)
        static constexpr auto condition()
        {
          return halp::enabled_when<&ins::release_mode, 0>{};
        }
        halp::item<&ins::release_velocity> release_velocity;
      } release_value;
    } release;
  };

  struct Note
  {
    uint8_t pitch{};
    uint8_t vel{};
    uint8_t chan{};
  };

  struct Pending
  {
    Note note;
    int64_t start{};  // absolute frame
    int64_t played{}; // absolute frame of the note-on as played
    int64_t length{-1}; // frames, once released before starting
    uint8_t off_vel{};
  };
  struct Running
  {
    Note note;
    int64_t end{-1}; // absolute frame; -1: until the note-off
    int64_t start{};
    uint8_t off_vel{};
  };
  //! A key held while Trigger is on: its note plays on release.
  struct Held
  {
    uint8_t vel{}; // 0: not held
    int64_t at{};
  };
  std::array<std::array<Held, 128>, 16> held{};
  bool stop_requested{};

  void stop() noexcept { stop_requested = true; }

  double gate() const noexcept { return std::max(0., double(inputs.gate.value)); }
  uint8_t fixed_release() const noexcept
  {
    return uint8_t(std::clamp(inputs.release_velocity.value, 0, 127));
  }
  //! A velocity lowered by Decay.
  uint8_t decayed(int vel) const noexcept
  {
    const double d = std::clamp(double(inputs.decay.value), 0., 1.);
    if(d == 0.)
      return uint8_t(vel);
    return uint8_t(std::clamp(int(std::lround(vel * (1. - d))), 1, 127));
  }
  std::vector<Pending> pending;
  std::vector<Running> running;
  int64_t last_end{-1};

  halp::setup setup;
  void prepare(halp::setup s)
  {
    setup = s;
    // A key (channel, pitch) is at most once in either list: they never grow
    // on the audio thread.
    pending.reserve(16 * 128);
    running.reserve(16 * 128);
  }

  //! How the grids map to frames during the current tick. Free, a grid is
  //! counted in frames from the start of playback; synced, in quarter notes of
  //! the timeline, so that it stays on the beats wherever playback started.
  struct Clock
  {
    int64_t pos{};
    double quarter_at_pos{};
    double frames_per_quarter{};
  } clock;

  void set_clock(const halp::tick_musical& tk) noexcept
  {
    clock.pos = tk.position_in_frames;
    clock.quarter_at_pos = tk.start_position_in_quarters;
    const double dq = tk.end_position_in_quarters - tk.start_position_in_quarters;
    if(dq > 0. && tk.frames > 0)
      clock.frames_per_quarter = tk.frames / dq;
    else if(tk.tempo > 0.)
      clock.frames_per_quarter = 60. / tk.tempo * setup.rate;
    else
      clock.frames_per_quarter = 0.;
  }

  bool musical(bool sync) const noexcept { return sync && clock.frames_per_quarter > 0.; }
  double frames_per_unit(bool sync) const noexcept
  {
    return musical(sync) ? clock.frames_per_quarter : 1.;
  }
  double to_unit(int64_t frame, bool sync) const noexcept
  {
    return musical(sync)
               ? clock.quarter_at_pos + (frame - clock.pos) / clock.frames_per_quarter
               : double(frame);
  }
  int64_t to_frame(double unit, bool sync) const noexcept
  {
    return musical(sync) ? clock.pos
                               + std::llround(
                                   (unit - clock.quarter_at_pos)
                                   * clock.frames_per_quarter)
                         : int64_t(std::llround(unit));
  }
  static constexpr double grid_epsilon = 1e-6;

  //! A time chooser's value, which is in seconds even when synced.
  double length(float seconds, bool sync) const noexcept
  {
    return std::max(0.f, seconds) * setup.rate / frames_per_unit(sync);
  }

  //! When a note played at `at` starts.
  int64_t start_of(int64_t at) const noexcept
  {
    const bool sync = inputs.grid.sync;
    const double g = length(inputs.grid.value, sync);
    if(g * frames_per_unit(sync) < 1.)
      return at;
    const double u = to_unit(at, sync);
    // The frame to musical time conversion is not exact: a note on a grid point
    // can land a hair after it, or before it.
    const double prev = std::floor(u / g + grid_epsilon) * g;
    const double window = (1. - std::clamp(inputs.tightness.value, 0.f, 1.f)) * g / 2.;
    if(u - prev <= window + grid_epsilon * g)
      return at; // on the grid point, or just missed it: on time
    return std::max(at, to_frame(prev + g, sync));
  }

  //! When a note starting at `start` ends, or -1 for its note-off.
  int64_t end_of(int64_t start) const noexcept
  {
    switch(inputs.length.value)
    {
      case UntilNoteOff:
        return -1;
      case FixedDuration:
        return start
               + int64_t(std::llround(std::max(0.f, inputs.duration.value) * setup.rate));
      case EndOnGrid: {
        const bool sync = inputs.duration.sync;
        const double d = length(inputs.duration.value, sync);
        if(d * frames_per_unit(sync) < 1.)
          return start;
        const double u = to_unit(start, sync);
        return std::max(start, to_frame((std::floor(u / d + grid_epsilon) + 1.) * d, sync));
      }
    }
    return -1;
  }

  void note_off(const Note& n, int64_t at, int64_t pos, uint8_t vel = 0)
  {
    auto m = libremidi::channel_events::note_off(n.chan, n.pitch, vel);
    m.timestamp = std::max<int64_t>(0, at - pos);
    outputs.midi.push_back(m);
  }

  //! Every sounding note off, at `at`, and nothing waits any more: a jump in
  //! time, a restart.
  void release_all(int64_t at, int64_t pos)
  {
    for(auto& r : running)
      note_off(r.note, at, pos, r.off_vel);
    running.clear();
    pending.clear();
  }

  using tick = halp::tick_flicks;
  void operator()(const tick& tk)
  {
    const int64_t pos = tk.position_in_frames;
    const int64_t end = pos + tk.frames;
    set_clock(tk);

    // Playback restarted or jumped back: nothing that was waiting still holds
    if(stop_requested || (last_end >= 0 && pos != last_end))
    {
      release_all(pos, pos);
      for(auto& c : held)
        c.fill({});
      stop_requested = false;
    }
    last_end = end;

    const bool from_input = inputs.release_mode.value == 1;
    for(const libremidi::message& in : inputs.midi)
    {
      if(!in.is_note_on_or_off())
      {
        if(mtk::is_all_notes_off(in))
        {
          release_all(pos + in.timestamp, pos);
          for(auto& c : held)
            c.fill({});
        }
        outputs.midi.push_back(in);
        continue;
      }

      const Note note{in[1], in[2], (uint8_t)in.get_channel()};
      const int64_t at = pos + in.timestamp;
      const bool on
          = in.get_message_type() == libremidi::message_type::NOTE_ON && note.vel != 0;
      auto same = [&](const auto& x) {
        return x.note.pitch == note.pitch && x.note.chan == note.chan;
      };

      auto& key = held[(note.chan - 1) & 15][note.pitch & 127];

      // The same key again: what was waiting is replaced, what sounds ends.
      // Returns the velocity of the key's latest note, or -1.
      auto replace = [&] {
        int previous = -1;
        for(auto it = pending.begin(); it != pending.end();)
          if(same(*it))
          {
            previous = it->note.vel;
            it = pending.erase(it);
          }
          else
            ++it;
        for(auto it = running.begin(); it != running.end();)
          if(same(*it))
          {
            if(previous < 0)
              previous = it->note.vel;
            note_off(it->note, at, pos, it->off_vel);
            it = running.erase(it);
          }
          else
            ++it;
        return previous;
      };

      if(on && inputs.trigger.value)
      {
        key = {note.vel, at};
      }
      else if(on)
      {
        const int previous = replace();
        Note n = note;
        if(previous > 0 && inputs.decay.value > 0)
          n.vel = decayed(previous);
        pending.push_back({n, start_of(at), at, -1, fixed_release()});
      }
      else if(inputs.trigger.value && key.vel > 0)
      {
        // The release plays the note, as long as the key was held with Until
        // note-off
        const int previous = replace();
        Note n = note;
        n.vel = inputs.decay.value > 0 ? decayed(previous > 0 ? previous : key.vel)
                                       : key.vel;
        const int64_t length
            = inputs.length.value == UntilNoteOff ? std::max<int64_t>(1, at - key.at) : -1;
        pending.push_back(
            {n, start_of(at), at, length, from_input ? note.vel : fixed_release()});
        key = {};
      }
      else
      {
        key = {};
        if(from_input)
        {
          for(auto& p : pending)
            if(same(p))
              p.off_vel = note.vel;
          for(auto& r : running)
            if(same(r))
              r.off_vel = note.vel;
        }
        if(inputs.length.value == UntilNoteOff)
        {
          // Not started yet: it will play the length it was played with.
          // Started: it ends now, or after Gate of its length.
          for(auto& p : pending)
            if(same(p) && p.length < 0)
              p.length = std::max<int64_t>(1, at - p.played);
          const double g = gate();
          for(auto it = running.begin(); it != running.end();)
          {
            if(same(*it) && (g == 1. || it->end < 0))
            {
              const int64_t stop
                  = it->start + std::llround((at - it->start) * g);
              if(g <= 1. || stop <= at)
              {
                note_off(it->note, at, pos, it->off_vel);
                it = running.erase(it);
                continue;
              }
              it->end = stop;
            }
            ++it;
          }
        }
        // The other lengths do not depend on the note-off
      }
    }

    // Notes whose start is in this tick
    for(auto it = pending.begin(); it != pending.end();)
    {
      if(it->start < end)
      {
        const int64_t start = std::max(it->start, pos);
        auto no = libremidi::channel_events::note_on(
            it->note.chan, it->note.pitch, it->note.vel);
        no.timestamp = start - pos;
        outputs.midi.push_back(no);
        const double g = gate();
        int64_t stop = it->length >= 0 ? start + it->length : end_of(start);
        if(stop >= 0 && g != 1.)
          stop = start + std::llround((stop - start) * g);
        running.push_back({it->note, stop, start, it->off_vel});
        it = pending.erase(it);
      }
      else
        ++it;
    }

    // Notes whose end is in this tick: after the note-ons at the same frame
    for(auto it = running.begin(); it != running.end();)
    {
      if(it->end >= 0 && it->end < end)
      {
        note_off(it->note, std::max(it->end, pos), pos, it->off_vel);
        it = running.erase(it);
      }
      else
        ++it;
    }
    // In time order; at the same frame, in the order they were made. An
    // insertion sort: stable without the buffer std::stable_sort allocates,
    // and the messages come nearly sorted already.
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
  }
};
}
