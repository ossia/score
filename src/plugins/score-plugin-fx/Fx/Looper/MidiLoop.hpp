#pragma once
#include <Fx/Looper/Transport.hpp>

#include <Advanced/MidiScaler/MidiCommon/Messages.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace Nodes::Looper
{
//! What the MIDI looper does with notes of earlier passes under an overdub.
enum class OverdubMode
{
  Merge,  ///< Every pass is heard.
  Replace ///< A pitch played in a pass silences that pitch of earlier ones
          ///< where the pass went.
};

//! A note still held at the end of the loop.
enum class HeldNotes
{
  Wrap,    ///< Carries on from the loop's start.
  Truncate ///< Ends with the loop.
};

struct midi_note
{
  //! Loop position of the note-on, in frames.
  int64_t start;
  //! Frames until the note-off; -1 while the key is held.
  int64_t length;
  uint8_t channel; //!< 1 - 16
  uint8_t pitch;
  uint8_t velocity;
  uint8_t pass;
  //! First pass that replaced it, -1 for none.
  int16_t replaced_by;
};

//! A controller, pitch bend or pressure value.
struct midi_control
{
  int64_t pos;
  uint8_t status;
  uint8_t data1;
  uint8_t data2;
  uint8_t pass;
  int16_t replaced_by;

  //! What two values have in common when they move the same thing.
  uint16_t key() const noexcept
  {
    const uint8_t kind = status & 0xF0;
    const bool indexed = kind == 0xB0 || kind == 0xA0;
    return uint16_t(status << 8 | (indexed ? data1 : 0));
  }
};

/**
 * The notes and controllers of a MIDI loop, in fixed storage.
 *
 * Every event is tagged with the pass that recorded it: undoing a pass hides
 * its events, redoing shows them again. Controllers of a pass replace the
 * same controller of earlier passes where the pass went, so two curves of one
 * controller never fight; notes do as well with OverdubMode::Replace.
 */
struct midi_loop
{
  static constexpr int max_notes = 4096;
  static constexpr int max_controls = 8192;
  static constexpr int max_passes = 255;
  static constexpr int max_touched = 64;

  std::array<midi_note, max_notes> notes{};
  std::array<midi_control, max_controls> controls{};
  int note_count{};
  int control_count{};

  //! Passes heard, and kept for redo.
  int count{};
  int total{};
  int64_t length{};
  bool full{};

  //! Length each pass repeats with: zero while it is the loop's length. A
  //! pass made before the loop was extended plays again over the longer loop.
  std::array<int64_t, max_passes + 1> period{};
  //! Length of the loop before and after each pass, for undo and redo.
  std::array<int64_t, max_passes + 1> loop_before{};
  std::array<int64_t, max_passes + 1> loop_after{};

  int64_t period_of(int pass) const noexcept
  {
    return period[pass] > 0 ? period[pass] : length;
  }

  //! The top pass goes on past the end of the loop: the loop grows by `unit`
  //! frames, the passes under it repeating. False when the loop would be
  //! longer than `max_length`.
  bool extend(int64_t unit, int64_t max_length) noexcept
  {
    if(count == 0 || unit <= 0 || length + unit > max_length)
      return false;
    for(int p = 0; p < count - 1; p++)
      if(period[p] == 0)
        period[p] = length;
    length += unit;
    return true;
  }

  // The pass being recorded: where it went, and what it touched.
  int64_t pass_start{};
  int64_t pass_written{};
  //! The frames the pass went over, not rounded: at a rate other than one, a
  //! tick goes over a fraction of a frame more than its whole frames.
  double pass_progress{};
  std::array<bool, 128> touched_pitches{};
  std::array<uint16_t, max_touched> touched_keys{};
  int touched_count{};
  bool taking{};

  bool empty() const noexcept { return count == 0 || length == 0; }

  bool in_pass(int64_t k) const noexcept
  {
    if(!taking)
      return false;
    if(pass_written >= length)
      return true;
    int64_t d = k - pass_start;
    if(d < 0)
      d += length;
    return d < pass_written;
  }

  bool touched(uint16_t key) const noexcept
  {
    for(int i = 0; i < touched_count; i++)
      if(touched_keys[i] == key)
        return true;
    return false;
  }

  bool replaced(int16_t by) const noexcept { return by >= 0 && by < count; }

  bool heard(const midi_note& n, OverdubMode mode) const noexcept
  {
    if(n.pass >= count || replaced(n.replaced_by))
      return false;
    if(taking && n.pass + 1 < count && mode == OverdubMode::Replace
       && touched_pitches[n.pitch] && in_pass(n.start))
      return false;
    return true;
  }

  bool heard(const midi_control& c) const noexcept
  {
    if(c.pass >= count || replaced(c.replaced_by))
      return false;
    if(taking && c.pass + 1 < count && touched(c.key()) && in_pass(c.pos))
      return false;
    return true;
  }

  void clear() noexcept
  {
    note_count = 0;
    control_count = 0;
    count = 0;
    total = 0;
    length = 0;
    full = false;
    taking = false;
  }

  //! Drops what redo would have brought back.
  void drop_redo() noexcept
  {
    note_count = int(
        std::remove_if(
            notes.begin(), notes.begin() + note_count,
            [this](const midi_note& n) { return n.pass >= count; })
        - notes.begin());
    control_count = int(
        std::remove_if(
            controls.begin(), controls.begin() + control_count,
            [this](const midi_control& c) { return c.pass >= count; })
        - controls.begin());
    for(int i = 0; i < note_count; i++)
      if(notes[i].replaced_by >= count)
        notes[i].replaced_by = -1;
    for(int i = 0; i < control_count; i++)
      if(controls[i].replaced_by >= count)
        controls[i].replaced_by = -1;
    total = count;
  }

  void begin_recording() noexcept
  {
    clear();
    count = 1;
    total = 1;
    period[0] = 0;
    begin_pass_state(0);
  }

  //! Starts an overdub pass at `start`. Past the last pass number, the
  //! oldest passes are merged into the first one.
  void begin_overdub(int64_t start) noexcept
  {
    drop_redo();
    if(count >= max_passes)
      merge_oldest();
    period[count] = 0;
    loop_before[count] = length;
    loop_after[count] = length;
    count++;
    total = count;
    begin_pass_state(start);
  }

  void merge_oldest() noexcept
  {
    for(int i = 0; i < note_count; i++)
    {
      if(notes[i].pass > 0)
        notes[i].pass--;
      if(notes[i].replaced_by > 0)
        notes[i].replaced_by--;
    }
    for(int i = 0; i < control_count; i++)
    {
      if(controls[i].pass > 0)
        controls[i].pass--;
      if(controls[i].replaced_by > 0)
        controls[i].replaced_by--;
    }
    // The merged pass repeats with the longer of the two periods; notes of
    // the shorter one are not repeated into it.
    period[0] = period[0] == 0 || period[1] == 0 ? 0 : std::max(period[0], period[1]);
    for(int p = 1; p + 1 < count; p++)
    {
      period[p] = period[p + 1];
      loop_before[p] = loop_before[p + 1];
      loop_after[p] = loop_after[p + 1];
    }
    count--;
  }

  void begin_pass_state(int64_t start) noexcept
  {
    pass_start = start;
    pass_written = 0;
    pass_progress = 0.;
    touched_pitches.fill(false);
    touched_count = 0;
    taking = true;
  }

  //! The pass is over: what it replaced stays replaced while it is heard.
  void end_pass(OverdubMode mode) noexcept
  {
    if(!taking)
      return;
    const int pass = count - 1;
    if(pass > 0)
    {
      for(int i = 0; i < note_count; i++)
      {
        auto& n = notes[i];
        if(n.pass < pass && mode == OverdubMode::Replace && touched_pitches[n.pitch]
           && in_pass(n.start) && n.replaced_by < 0)
          n.replaced_by = int16_t(pass);
      }
      for(int i = 0; i < control_count; i++)
      {
        auto& c = controls[i];
        if(c.pass < pass && touched(c.key()) && in_pass(c.pos) && c.replaced_by < 0)
          c.replaced_by = int16_t(pass);
      }
    }
    taking = false;
  }

  //! The pass went over `frames` more frames of the loop.
  void advance_pass(double frames) noexcept
  {
    pass_progress += frames;
    pass_written = int64_t(std::floor(pass_progress + 1e-6));
  }

  //! Copies what another loop holds, without its unused storage.
  void assign(const midi_loop& o) noexcept
  {
    std::copy_n(o.notes.begin(), o.note_count, notes.begin());
    std::copy_n(o.controls.begin(), o.control_count, controls.begin());
    note_count = o.note_count;
    control_count = o.control_count;
    count = o.count;
    total = o.total;
    length = o.length;
    full = o.full;
    period = o.period;
    loop_before = o.loop_before;
    loop_after = o.loop_after;
    pass_start = o.pass_start;
    pass_written = o.pass_written;
    pass_progress = o.pass_progress;
    touched_pitches = o.touched_pitches;
    touched_keys = o.touched_keys;
    touched_count = o.touched_count;
    taking = o.taking;
  }

  bool undo() noexcept
  {
    if(count == 0)
      return false;
    if(count > 1)
    {
      loop_after[count - 1] = length;
      length = loop_before[count - 1];
    }
    count--;
    return true;
  }

  bool redo() noexcept
  {
    if(count >= total)
      return false;
    if(count > 0)
      length = loop_after[count];
    count++;
    return true;
  }

  midi_note* add_note(int64_t pos, int channel, int pitch, int velocity) noexcept
  {
    if(note_count >= max_notes)
    {
      full = true;
      return nullptr;
    }
    touched_pitches[pitch & 127] = true;
    auto& n = notes[note_count++];
    n = midi_note{
        .start = pos,
        .length = -1,
        .channel = uint8_t(channel),
        .pitch = uint8_t(pitch),
        .velocity = uint8_t(velocity),
        .pass = uint8_t(count - 1),
        .replaced_by = -1};
    return &n;
  }

  //! The note-off of a held note: the oldest one of that key still open.
  midi_note* open_note(int channel, int pitch) noexcept
  {
    for(int i = 0; i < note_count; i++)
    {
      auto& n = notes[i];
      if(n.length < 0 && n.channel == channel && n.pitch == pitch)
        return &n;
    }
    return nullptr;
  }

  void add_control(int64_t pos, uint8_t status, uint8_t d1, uint8_t d2) noexcept
  {
    if(control_count >= max_controls)
    {
      full = true;
      return;
    }
    auto& c = controls[control_count++];
    c = midi_control{
        .pos = pos,
        .status = status,
        .data1 = d1,
        .data2 = d2,
        .pass = uint8_t(count - 1),
        .replaced_by = -1};
    const auto key = c.key();
    if(!touched(key) && touched_count < max_touched)
      touched_keys[touched_count++] = key;
  }

  //! Notes still held when the loop is left get the length they have now.
  void close_open_notes(int64_t pos) noexcept
  {
    for(int i = 0; i < note_count; i++)
    {
      auto& n = notes[i];
      if(n.length < 0)
        n.length = distance(n.start, pos);
    }
  }

  int64_t distance(int64_t from, int64_t to) const noexcept
  {
    if(length <= 0)
      return std::max<int64_t>(1, to - from);
    int64_t d = (to - from) % length;
    if(d <= 0)
      d += length;
    return d;
  }
};
}
