#pragma once
#include <Fx/Quantifier.hpp>

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
  halp_meta(category, "Midi")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/midi-utilities.html#quantifier")
  halp_meta(author, "ossia score")
  halp_meta(description, "Quantifies a MIDI input")
  halp_meta(uuid, "a0cb1144-05a8-4f9b-b22c-81cd964314fb")

  struct
  {
    halp::midi_bus<"in", libremidi::message> midi;
    //! Notes start on this grid; 0 lets them through as they come.
    halp::time_chooser<"Grid", halp::range{0., 4., 0.125}> grid;
    //! 1: a note always waits for the next grid point. Lower: a note played
    //! just after a grid point -- by up to (1 - tightness) of half a step --
    //! counts as on time, and starts at once.
    halp::hslider_f32<"Tightness", halp::range{0.f, 1.f, 0.8f}> tightness;
    halp::enum_t<NoteLength, "Length"> length;
    //! The length of a note, or the grid its end falls on.
    halp::time_chooser<"Duration", halp::range{0., 8., 0.25}> duration;
  } inputs;
  struct
  {
    midi_out midi;
  } outputs;

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
  };
  struct Running
  {
    Note note;
    int64_t end{-1}; // absolute frame; -1: until the note-off
  };
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
    const double prev = std::floor(u / g) * g;
    const double window = (1. - std::clamp(inputs.tightness.value, 0.f, 1.f)) * g / 2.;
    if(u - prev <= window)
      return at; // just missed the grid point: on time
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
        return std::max(start, to_frame((std::floor(u / d) + 1.) * d, sync));
      }
    }
    return -1;
  }

  void note_off(const Note& n, int64_t at, int64_t pos)
  {
    auto m = libremidi::channel_events::note_off(n.chan, n.pitch, 0);
    m.timestamp = std::max<int64_t>(0, at - pos);
    outputs.midi.push_back(m);
  }

  //! Every sounding note off, at `at`, and nothing waits any more: a jump in
  //! time, a restart.
  void release_all(int64_t at, int64_t pos)
  {
    for(auto& r : running)
      note_off(r.note, at, pos);
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
    if(last_end >= 0 && pos != last_end)
      release_all(pos, pos);
    last_end = end;

    for(const libremidi::message& in : inputs.midi)
    {
      if(!in.is_note_on_or_off())
      {
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

      if(on)
      {
        // The same key again: what was waiting is replaced, what sounds ends
        std::erase_if(pending, same);
        for(auto it = running.begin(); it != running.end();)
          if(same(*it))
          {
            note_off(it->note, at, pos);
            it = running.erase(it);
          }
          else
            ++it;
        pending.push_back({note, start_of(at), at, -1});
      }
      else if(inputs.length.value == UntilNoteOff)
      {
        // Not started yet: it will play the length it was played with.
        // Started: it ends now.
        for(auto& p : pending)
          if(same(p) && p.length < 0)
            p.length = std::max<int64_t>(1, at - p.played);
        for(auto it = running.begin(); it != running.end();)
          if(same(*it))
          {
            note_off(it->note, at, pos);
            it = running.erase(it);
          }
          else
            ++it;
      }
      // The other lengths do not depend on the note-off
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
        const int64_t stop = it->length >= 0 ? start + it->length : end_of(start);
        running.push_back({it->note, stop});
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
        note_off(it->note, std::max(it->end, pos), pos);
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
