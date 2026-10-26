#pragma once
#include <Fx/Looper/PassStack.hpp>

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

//! What the MIDI loop keeps of a pass besides its events.
struct midi_pass
{
  //! Length it repeats with: zero while it is the loop's length. A pass made
  //! before the loop was extended plays again over the longer loop.
  int64_t period{};
  //! Length of the loop before and after the pass, for undo and redo.
  int64_t loop_before{};
  int64_t loop_after{};
  //! Kept but not heard: as if erased for what the loop sounds like, so it
  //! replaces nothing either.
  bool muted{};
};

/**
 * The notes and controllers of a MIDI loop, in fixed storage.
 *
 * The passes are a pass_stack; every event is tagged with the pass that
 * recorded it: undoing a pass hides its events, redoing shows them again,
 * erasing it removes them. Controllers of a pass replace the same controller
 * of earlier passes where the pass went, so two curves of one controller
 * never fight; notes do as well with OverdubMode::Replace. Each event keeps
 * every pass that replaces it, and is heard when none of them is: muting,
 * erasing or merging a pass leaves what the others replace as it is.
 */
struct midi_loop : pass_stack<midi_loop, midi_pass, 32>
{
  static constexpr int max_notes = 4096;
  static constexpr int max_controls = 8192;
  static constexpr int max_touched = 64;

  using tags = tagged_material<max_passes>;
  //! Where the events of a pass set aside are, past every pass there can be.
  static constexpr int aside_pass = tags::aside_pass;

  std::array<midi_note, max_notes> notes{};
  std::array<midi_control, max_controls> controls{};
  int note_count{};
  int control_count{};
  bool full{};

  //! Changes each time what is heard of the controllers may have changed.
  uint64_t edits{};

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

  // pass_stack's hooks: the events move with their passes.
  void passes_changed() noexcept { edits++; }
  bool may_remove(int k) const noexcept { return !(taking && k == count - 1); }
  void muting(int) noexcept { }
  void removing(int) noexcept { }
  void erased(int k) noexcept
  {
    remove_events([k](const auto& e) { return e.pass == k; });
    for_each_event([k](auto& e) { tags::erase(e, k); });
  }
  void aside_taken(int k) noexcept
  {
    for_each_event([k](auto& e) { tags::set_aside(e, k); });
  }
  void aside_returned(int k) noexcept
  {
    for_each_event([k](auto& e) { tags::put_back(e, k); });
  }
  void aside_forgotten() noexcept
  {
    remove_events([](const auto& e) { return e.pass == aside_pass; });
    for_each_event([](auto& e) { e.replaced_by &= ~tags::bit(aside_pass); });
  }
  void redo_dropped(bool keep_aside) noexcept
  {
    const int heard_passes = count;
    remove_events([heard_passes, keep_aside](const auto& e) {
      return e.pass >= heard_passes && !(keep_aside && e.pass == aside_pass);
    });
    uint64_t kept = count >= 64 ? ~uint64_t(0) : (uint64_t(1) << count) - 1;
    if(keep_aside)
      kept |= tags::bit(aside_pass);
    for_each_event([kept](auto& e) { e.replaced_by &= kept; });
  }
  void unmerge_dropped() noexcept
  {
    remove_events([](const auto& e) { return e.pass == tags::lost_pass; });
  }
  void unmerged(int at) noexcept
  {
    for_each_event([at](auto& e) { tags::unmerge(e, at); });
  }
  void cleared() noexcept
  {
    note_count = 0;
    control_count = 0;
    full = false;
    taking = false;
  }
  void undoing() noexcept { }

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

  //! The pass being taken in replaces what is under it as it goes, unless it
  //! is muted.
  bool replacing() const noexcept
  {
    return taking && count > 0 && !layers[count - 1].muted;
  }

  //! `own_mute` false hears a muted pass's events as if it were not muted.
  bool heard(const midi_note& n, OverdubMode mode, bool own_mute = true) const noexcept
  {
    if(n.pass >= count || replaced(n.replaced_by, n.pass, taking)
       || (own_mute && layers[n.pass].muted))
      return false;
    if(replacing() && n.pass + 1 < count && mode == OverdubMode::Replace
       && touched_pitches[n.pitch] && in_pass(n.start))
      return false;
    return true;
  }

  bool heard(const midi_control& c, bool own_mute = true) const noexcept
  {
    if(c.pass >= count || replaced(c.replaced_by, c.pass, taking)
       || (own_mute && layers[c.pass].muted))
      return false;
    if(replacing() && c.pass + 1 < count && touched(c.key()) && in_pass(c.pos))
      return false;
    return true;
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

  void begin_recording() noexcept
  {
    begin_first_pass();
    begin_pass_state(0);
  }

  //! Starts an overdub pass at `start`. With `layers` passes, the two oldest
  //! are merged first when `oldest` is set; else the pass is one more, merged
  //! into the newest when it ends. `replace`, a pass heard, makes room for it
  //! instead: it is set aside, undo putting it back.
  void begin_overdub(
      int64_t start, int max = max_layers, bool oldest = true, int replace = -1) noexcept
  {
    drop_redo();
    forget_aside();
    if(replace >= 0)
      set_aside(replace);
    max = std::clamp(max, 2, max_layers);
    if(oldest)
    {
      while(count >= max)
        merge(0);
    }
    else
    {
      // The pass before it was not merged yet.
      while(count > max)
        merge(count - 2);
    }
    push_pass();
    begin_pass_state(start);
  }

  //! Merges pass `at` and the one over it into one, heard as the two were.
  //! A muted pass merged with one that is not is left out of the merged one:
  //! merging is where it is lost. With `undoable`, undo takes the upper pass
  //! out again, as long as nothing else changes the passes.
  void merge(int at, bool undoable = false) noexcept
  {
    const int a = at, b = at + 1;
    if(a < 0 || b >= count)
      return;
    begin_merge();
    const midi_pass was_a = layers[a], was_b = layers[b];
    const bool lost_a = was_a.muted && !was_b.muted;
    const bool lost_b = was_b.muted && !was_a.muted;
    for_each_event([=](auto& e) { tags::merge(e, a, lost_a, lost_b); });
    merged(a, merged_pass(was_a, was_b), undoable, was_a, was_b);
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
    layers = o.layers;
    edits = o.edits;
    unmerge = o.unmerge;
    aside = o.aside;
    pass_start = o.pass_start;
    pass_written = o.pass_written;
    pass_progress = o.pass_progress;
    pass_backwards = o.pass_backwards;
    touched_pitches = o.touched_pitches;
    touched_keys = o.touched_keys;
    touched_count = o.touched_count;
    taking = o.taking;
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
