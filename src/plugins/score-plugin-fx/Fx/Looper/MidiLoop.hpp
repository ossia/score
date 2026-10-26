#pragma once
#include <Fx/Looper/Transport.hpp>

#include <Advanced/MidiScaler/MidiCommon/Messages.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <utility>

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

//! What became of an event when the last pass was merged into the newest
//! one, for undo to take it out again.
enum merge_mark : uint8_t
{
  from_merged = 1,     ///< It is an event of the pass merged.
  replaced_by_merged = 2, ///< Only the pass merged replaced it.
  replaced_by_both = 4  ///< The newest pass replaced it as well.
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
  //! The passes that replace it, a bit each.
  uint64_t replaced_by;
  uint8_t merged;
};

//! A controller, pitch bend or pressure value.
struct midi_control
{
  int64_t pos;
  uint8_t status;
  uint8_t data1;
  uint8_t data2;
  uint8_t pass;
  uint64_t replaced_by;
  uint8_t merged;

  //! What two values have in common when they move the same thing.
  uint16_t key() const noexcept
  {
    const uint8_t kind = status & 0xF0;
    const bool indexed = kind == 0xB0 || kind == 0xA0;
    return uint16_t(status << 8 | (indexed ? data1 : 0));
  }
};

//! A set of passes, a bit each, without pass k: the passes over it move
//! down a place.
inline constexpr uint64_t without_pass(uint64_t m, int k) noexcept
{
  const uint64_t low = m & ((uint64_t(1) << k) - 1);
  return low | ((m >> (k + 1)) << k);
}

//! A set of passes with room made for a pass at k: the passes from k on move
//! up a place.
inline constexpr uint64_t with_room_at(uint64_t m, int k) noexcept
{
  const uint64_t low = m & ((uint64_t(1) << k) - 1);
  return low | ((m >> k) << (k + 1));
}

/**
 * The notes and controllers of a MIDI loop, in fixed storage.
 *
 * Every event is tagged with the pass that recorded it: undoing a pass hides
 * its events, redoing shows them again, erasing it removes them. Controllers
 * of a pass replace the same controller of earlier passes where the pass
 * went, so two curves of one controller never fight; notes do as well with
 * OverdubMode::Replace. Each event keeps every pass that replaces it, and is
 * heard when none of them is: muting, erasing or merging a pass leaves what
 * the others replace as it is.
 */
struct midi_loop
{
  static constexpr int max_notes = 4096;
  static constexpr int max_controls = 8192;
  //! Most layers kept: Max layers is at most this.
  static constexpr int max_layers = 32;
  //! A pass past Max layers is one more until it is merged into the newest.
  static constexpr int max_passes = max_layers + 1;
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
  //! Passes kept but not heard: as if erased for what the loop sounds like,
  //! so they replace nothing either.
  std::array<bool, max_passes + 1> muted{};
  //! Changes each time what is heard of the controllers may have changed.
  uint64_t edits{};
  //! Counts the passes merged, which moves the ones over them down a place.
  uint64_t merges{};

  //! The last pass merged into the newest one, which undo takes out again.
  struct unmerge_state
  {
    bool valid{};
    int at{};
    int64_t period{};
    int64_t loop_before{};
    int64_t loop_after{};
    int64_t newest_loop_after{};
  } unmerge;

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
  //! The playhead goes backwards: the part the pass went over grows towards
  //! the loop's start, pass_start being its first position.
  bool pass_backwards{};
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

  //! One of the passes of `by` replaces an event of pass `self` now: it is
  //! heard, not muted, and over. The pass being taken in replaces as it goes,
  //! not through this; the event's own pass, for a pass made of two merged
  //! ones, replaces it whatever.
  bool replaced(uint64_t by, int self) const noexcept
  {
    for(uint64_t m = by; m != 0; m &= m - 1)
    {
      const int q = std::countr_zero(m);
      if(q == self)
        return true;
      if(q < count && !muted[q] && !(taking && q == count - 1))
        return true;
    }
    return false;
  }

  //! The pass being taken in replaces what is under it as it goes, unless it
  //! is muted.
  bool replacing() const noexcept { return taking && count > 0 && !muted[count - 1]; }

  //! `own_mute` false hears a muted pass's events as if it were not muted.
  bool heard(const midi_note& n, OverdubMode mode, bool own_mute = true) const noexcept
  {
    if(n.pass >= count || replaced(n.replaced_by, n.pass) || (own_mute && muted[n.pass]))
      return false;
    if(replacing() && n.pass + 1 < count && mode == OverdubMode::Replace
       && touched_pitches[n.pitch] && in_pass(n.start))
      return false;
    return true;
  }

  bool heard(const midi_control& c, bool own_mute = true) const noexcept
  {
    if(c.pass >= count || replaced(c.replaced_by, c.pass) || (own_mute && muted[c.pass]))
      return false;
    if(replacing() && c.pass + 1 < count && touched(c.key()) && in_pass(c.pos))
      return false;
    return true;
  }

  //! Mutes or unmutes pass k: what it replaced is heard while it is muted,
  //! unless another pass replaces it as well.
  bool set_muted(int k, bool m) noexcept
  {
    if(k < 0 || k >= count || muted[k] == m)
      return false;
    muted[k] = m;
    unmerge.valid = false;
    edits++;
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
    unmerge.valid = false;
    edits++;
  }

  template <typename F>
  void remove_events(F&& gone) noexcept
  {
    note_count = int(
        std::remove_if(notes.begin(), notes.begin() + note_count, gone) - notes.begin());
    control_count = int(
        std::remove_if(controls.begin(), controls.begin() + control_count, gone)
        - controls.begin());
  }

  template <typename F>
  void for_each_event(F&& f) noexcept
  {
    for(int i = 0; i < note_count; i++)
      f(notes[i]);
    for(int i = 0; i < control_count; i++)
      f(controls[i]);
  }

  //! Drops what redo would have brought back.
  void drop_redo() noexcept
  {
    const int heard_passes = count;
    remove_events([heard_passes](const auto& e) { return e.pass >= heard_passes; });
    const uint64_t kept = count >= 64 ? ~uint64_t(0) : (uint64_t(1) << count) - 1;
    for_each_event([kept](auto& e) { e.replaced_by &= kept; });
    total = count;
    unmerge.valid = false;
    edits++;
  }

  void begin_recording() noexcept
  {
    clear();
    count = 1;
    total = 1;
    period[0] = 0;
    muted[0] = false;
    begin_pass_state(0);
  }

  //! Starts an overdub pass at `start`. With `layers` passes, the two oldest
  //! are merged first when `oldest` is set; else the pass is one more, merged
  //! into the newest when it ends.
  void begin_overdub(int64_t start, int layers = max_layers, bool oldest = true) noexcept
  {
    drop_redo();
    layers = std::clamp(layers, 2, max_layers);
    if(oldest)
    {
      while(count >= layers)
        merge(0);
    }
    else
    {
      // The pass before it was not merged yet.
      while(count > layers)
        merge(count - 2);
    }
    period[count] = 0;
    loop_before[count] = length;
    loop_after[count] = length;
    muted[count] = false;
    count++;
    total = count;
    begin_pass_state(start);
  }

  //! Merges pass `at` and the one over it into one, heard as the two were.
  //! A muted pass is left out of the merged one: merging is where it is
  //! lost. With `undoable`, undo takes the upper pass out again, as long as
  //! nothing else changes the passes.
  void merge(int at, bool undoable = false) noexcept
  {
    const int a = at, b = at + 1;
    if(a < 0 || b >= count)
      return;
    edits++;
    merges++;
    unmerge.valid = false;
    if(muted[a] != muted[b])
    {
      const int lost = muted[a] ? a : b;
      remove_events([lost](const auto& e) { return e.pass == lost; });
      muted[a] = false;
      undoable = false;
    }
    else if(muted[a])
    {
      undoable = false;
    }
    if(undoable)
      unmerge = unmerge_state{
          .valid = true,
          .at = a,
          .period = period[b],
          .loop_before = loop_before[b],
          .loop_after = loop_after[b],
          .newest_loop_after = loop_after[a]};

    const uint64_t bit_a = uint64_t(1) << a, bit_b = uint64_t(1) << b;
    for_each_event([=](auto& e) {
      uint8_t mark = 0;
      if(e.pass == b)
        mark |= from_merged;
      if(e.replaced_by & bit_b)
        mark |= (e.replaced_by & bit_a) ? replaced_by_both : replaced_by_merged;
      e.merged = mark;
      // What the upper pass replaces, the merged one does: its own events
      // under it included, which stay silent.
      if(e.replaced_by & bit_b)
        e.replaced_by |= bit_a;
      e.replaced_by = without_pass(e.replaced_by, b);
      if(e.pass >= b)
        e.pass--;
    });

    period[a] = period[a] == 0 || period[b] == 0 ? 0 : std::max(period[a], period[b]);
    loop_after[a] = loop_after[b];
    for(int p = b; p + 1 < count; p++)
    {
      period[p] = period[p + 1];
      loop_before[p] = loop_before[p + 1];
      loop_after[p] = loop_after[p + 1];
      muted[p] = muted[p + 1];
    }
    count--;
    total = count;
  }

  void begin_pass_state(int64_t start) noexcept
  {
    pass_start = start;
    pass_written = 0;
    pass_progress = 0.;
    pass_backwards = false;
    touched_pitches.fill(false);
    touched_count = 0;
    taking = true;
  }

  //! The pass is over: what it went over is replaced where it played the
  //! same pitch, with OverdubMode::Replace, or moved the same controller.
  void end_pass(OverdubMode mode) noexcept
  {
    if(!taking)
      return;
    const int pass = count - 1;
    edits++;
    if(pass > 0)
    {
      const uint64_t bit = uint64_t(1) << pass;
      for(int i = 0; i < note_count; i++)
      {
        auto& n = notes[i];
        if(n.pass < pass && mode == OverdubMode::Replace && touched_pitches[n.pitch]
           && in_pass(n.start))
          n.replaced_by |= bit;
      }
      for(int i = 0; i < control_count; i++)
      {
        auto& c = controls[i];
        if(c.pass < pass && touched(c.key()) && in_pass(c.pos))
          c.replaced_by |= bit;
      }
    }
    taking = false;
  }

  //! The pass went over `frames` more frames of the loop, backwards when
  //! the playhead goes backwards.
  void advance_pass(double frames, bool backwards = false) noexcept
  {
    pass_progress += frames;
    const auto written = int64_t(std::floor(pass_progress + 1e-6));
    if(backwards && length > 0)
    {
      pass_start = (pass_start - (written - pass_written)) % length;
      if(pass_start < 0)
        pass_start += length;
    }
    pass_backwards = backwards;
    pass_written = written;
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
    muted = o.muted;
    edits = o.edits;
    merges = o.merges;
    unmerge = o.unmerge;
    pass_start = o.pass_start;
    pass_written = o.pass_written;
    pass_progress = o.pass_progress;
    pass_backwards = o.pass_backwards;
    touched_pitches = o.touched_pitches;
    touched_keys = o.touched_keys;
    touched_count = o.touched_count;
    taking = o.taking;
  }

  bool undo() noexcept
  {
    if(count == 0)
      return false;
    edits++;
    // The pass merged into the newest one comes out of it.
    if(std::exchange(unmerge.valid, false) && unmerge.at == count - 1 && !taking)
    {
      const int a = unmerge.at, b = a + 1;
      for_each_event([=](auto& e) {
        e.replaced_by = with_room_at(e.replaced_by, b);
        if(e.merged & from_merged)
          e.pass = uint8_t(b);
        if(e.merged & replaced_by_merged)
          e.replaced_by = (e.replaced_by & ~(uint64_t(1) << a)) | (uint64_t(1) << b);
        if(e.merged & replaced_by_both)
          e.replaced_by |= uint64_t(1) << b;
        e.merged = 0;
      });
      period[b] = unmerge.period;
      loop_before[b] = unmerge.loop_before;
      loop_after[b] = length;
      loop_after[a] = unmerge.newest_loop_after;
      muted[b] = false;
      length = unmerge.loop_before;
      total = count + 1;
      return true;
    }
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
    edits++;
    unmerge.valid = false;
    if(count > 0)
      length = loop_after[count];
    count++;
    return true;
  }

  //! Takes pass k out of the loop, k being heard, not the only pass heard and
  //! not one being taken in: its events go, the passes over it and the ones
  //! redo brings back move down a place, and the loop keeps its length.
  bool erase(int k) noexcept
  {
    if(k < 0 || k >= count || count < 2 || (taking && k == count - 1))
      return false;
    edits++;
    unmerge.valid = false;
    remove_events([k](const auto& e) { return e.pass == k; });
    for_each_event([k](auto& e) {
      e.replaced_by = without_pass(e.replaced_by, k);
      if(e.pass > k)
        e.pass--;
    });
    for(int p = k; p + 1 < total; p++)
    {
      period[p] = period[p + 1];
      loop_before[p] = loop_before[p + 1];
      loop_after[p] = loop_after[p + 1];
      muted[p] = muted[p + 1];
    }
    count--;
    total--;
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
        .replaced_by = 0,
        .merged = 0};
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
    edits++;
    auto& c = controls[control_count++];
    c = midi_control{
        .pos = pos,
        .status = status,
        .data1 = d1,
        .data2 = d2,
        .pass = uint8_t(count - 1),
        .replaced_by = 0,
        .merged = 0};
    const auto key = c.key();
    if(!touched(key) && touched_count < max_touched)
      touched_keys[touched_count++] = key;
  }

  //! The pass moves a controller it has no value of yet: it replaces it.
  void touch(uint16_t key) noexcept
  {
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
