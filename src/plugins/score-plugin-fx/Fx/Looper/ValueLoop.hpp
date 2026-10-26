#pragma once
#include <Fx/Looper/PassStack.hpp>

#include <ossia/network/value/value.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <utility>

namespace Nodes::Looper
{
/**
 * Where the value looper keeps a value: every one it records, holds or sends
 * is copied, moved and let go of through this type and nowhere else.
 *
 * A value is kept as a plain ossia::value. Numbers, booleans, impulses and
 * vectors live inside it, so that keeping, copying and dropping them never
 * allocates; a string, a list or a map owns heap memory, which is allocated
 * when it is copied in and freed when it is let go of, on whatever thread
 * does it, the audio thread included. A pooled allocator for those is a
 * change of this type alone.
 */
class value_slot
{
public:
  value_slot() noexcept = default;
  value_slot(const value_slot& o) { m_value = o.m_value; }
  value_slot(value_slot&& o) noexcept { m_value = std::move(o.m_value); }
  value_slot& operator=(const value_slot& o)
  {
    if(this != &o)
      m_value = o.m_value;
    return *this;
  }
  value_slot& operator=(value_slot&& o) noexcept
  {
    if(this != &o)
      m_value = std::move(o.m_value);
    return *this;
  }
  ~value_slot() = default;

  void set(const ossia::value& v) { m_value = v; }
  void set(ossia::value&& v) noexcept { m_value = std::move(v); }
  //! Lets go of what it holds.
  void reset() noexcept { m_value = ossia::value{}; }

  const ossia::value& get() const noexcept { return m_value; }
  bool empty() const noexcept { return !m_value.valid(); }
  bool impulse() const noexcept { return m_value.get_type() == ossia::val_type::IMPULSE; }

  //! The same value: of the same type and equal, a list or a map element by
  //! element. An impulse is never the same as anything else.
  static bool same(const ossia::value& a, const ossia::value& b) noexcept
  {
    return a.get_type() == b.get_type() && a == b;
  }
  bool same(const ossia::value& o) const noexcept { return same(m_value, o); }
  bool same(const value_slot& o) const noexcept { return same(m_value, o.m_value); }

private:
  ossia::value m_value;
};

//! A value of the loop.
struct value_event
{
  //! Loop position, in frames.
  int64_t pos;
  //! How far into its pass it was recorded: a pass going round the loop
  //! again writes over what it recorded a round before.
  int64_t written;
  value_slot value;
  //! The pass that recorded it, by the order the passes were made in: the
  //! passes merged into one keep theirs apart.
  uint32_t serial;
  //! What the value is of: one value stream for now, so always 0.
  uint16_t key;
  //! Fired where it is, rather than held until the next one.
  bool impulse;
  uint8_t pass;
  //! The passes that replace it, a bit each.
  uint64_t replaced_by;
  uint8_t merged;
};

//! Where a pass holds a key: from its first new value of the key to the end
//! of the pass, `span` frames from `from`. A span as long as the pass's
//! period holds the key all round.
struct value_stretch
{
  int64_t from;
  int64_t span;
  uint32_t serial;
  uint16_t key;
  uint8_t pass;
  uint64_t replaced_by;
  uint8_t merged;
};

/**
 * The values of a loop, in passes, as a pass_stack whose material is the
 * values and the stretches where each pass holds a key.
 *
 * An impulse is fired where it was recorded, as a note: every pass is heard
 * over the others. Any other value is held until the next one of its key:
 * a pass holds a key from its first new value of it to its end, and, where
 * it does, what the passes under it hold of that key is not heard. Before
 * that first value, and after the pass, the passes under it are heard again.
 *
 * The metadata of the values is in fixed storage; a value itself may own
 * heap memory, see value_slot.
 */
struct value_loop : tagged_pass_stack<value_loop, tagged_pass, 32>
{
  static constexpr int max_events = 8192;
  static constexpr int max_stretches = 512;
  static constexpr int max_touched = 16;

  std::array<value_event, max_events> events{};
  std::array<value_stretch, max_stretches> stretches{};
  int event_count{};
  int stretch_count{};
  bool full{};
  //! Numbers the passes in the order they are made.
  uint32_t next_serial{};

  //! A key the pass being taken in holds: where, and how far into the pass,
  //! it had its first new value, the way the playhead went then, and the
  //! stretch it holds it over.
  struct touch
  {
    uint16_t key;
    int64_t first;
    int64_t at;
    bool backwards;
    int stretch;
  };
  std::array<touch, max_touched> touches{};
  int touch_count{};
  //! The serial of the pass being taken in.
  uint32_t serial{};
  //! The pass being taken in is a recording: it holds what it has all
  //! round the loop.
  bool whole{};

  // tagged_pass_stack's hooks: the values and the stretches are the
  // material.
  void clear_material() noexcept
  {
    for(int i = 0; i < event_count; i++)
      events[i].value.reset();
    event_count = 0;
    stretch_count = 0;
    full = false;
    next_serial = 0;
  }
  void pass_state_begun() noexcept
  {
    touch_count = 0;
    serial = next_serial++;
    whole = false;
  }

  void begin_recording() noexcept
  {
    tagged_pass_stack::begin_recording();
    whole = true;
  }
  template <typename F>
  void for_each_material(F&& f) noexcept
  {
    for(int i = 0; i < event_count; i++)
      f(events[i]);
    for(int i = 0; i < stretch_count; i++)
      f(stretches[i]);
  }
  template <typename F>
  void remove_material(F&& gone) noexcept
  {
    remove_events(gone);
    stretch_count = int(
        std::remove_if(stretches.begin(), stretches.begin() + stretch_count, gone)
        - stretches.begin());
    for(int i = 0; i < touch_count; i++)
      touches[i].stretch = find_stretch(touches[i].key);
  }

  //! The values `gone` says are let go of.
  template <typename F>
  void remove_events(F&& gone) noexcept
  {
    int out = 0;
    for(int i = 0; i < event_count; i++)
    {
      if(gone(events[i]))
        continue;
      if(out != i)
        events[out] = std::move(events[i]);
      out++;
    }
    for(int i = out; i < event_count; i++)
      events[i].value.reset();
    if(out != event_count)
      edits++;
    event_count = out;
  }

  //! Heard, the pass being taken in included. `own_mute` false hears a muted
  //! pass's material as if it were not muted.
  template <typename T>
  bool heard(const T& e, bool own_mute = true) const noexcept
  {
    return e.pass < count && !replaced(e.replaced_by, e.pass, false)
           && !(own_mute && layers[e.pass].muted);
  }

  //! The open stretch the pass being taken in holds `key` over, -1 for none.
  int find_stretch(uint16_t key) const noexcept
  {
    if(!taking || count == 0)
      return -1;
    for(int i = stretch_count - 1; i >= 0; i--)
      if(stretches[i].key == key && stretches[i].serial == serial
         && stretches[i].pass == count - 1)
        return i;
    return -1;
  }

  const touch* touched(uint16_t key) const noexcept
  {
    for(int i = 0; i < touch_count; i++)
      if(touches[i].key == key)
        return &touches[i];
    return nullptr;
  }

  //! The pass being taken in has its first new value of `key` at `pos`,
  //! `at` frames into it: it holds the key from there on.
  void touch_key(uint16_t key, int64_t pos, int64_t at, bool backwards) noexcept
  {
    if(touched(key) || touch_count >= max_touched || stretch_count >= max_stretches)
      return;
    auto& s = stretches[stretch_count];
    s = value_stretch{
        .from = pos,
        .span = 0,
        .serial = serial,
        .key = key,
        .pass = uint8_t(count - 1),
        .replaced_by = 0,
        .merged = 0};
    touches[touch_count++] = touch{key, pos, at, backwards, stretch_count};
    stretch_count++;
    update_open_stretches();
  }

  //! The stretches of the pass being taken in, as far as it will go: from
  //! the first new value of each key to where the pass started, round the
  //! loop, or all round once it went round. What it has not reached yet is
  //! held as well, since it holds it when it reaches it; the pass ending
  //! makes them as long as it went.
  void update_open_stretches() noexcept
  {
    for(int i = 0; i < touch_count; i++)
    {
      const auto& t = touches[i];
      if(t.stretch < 0)
        continue;
      auto& s = stretches[t.stretch];
      const int64_t span = std::max<int64_t>(0, length - t.at);
      set_stretch(s, t, pass_written >= length || whole ? length : span);
    }
  }

  void set_stretch(value_stretch& s, const touch& t, int64_t span) noexcept
  {
    int64_t from = t.backwards ? t.first - span : t.first;
    if(length > 0)
    {
      from %= length;
      if(from < 0)
        from += length;
    }
    if(s.from != from || s.span != span)
      edits++;
    s.from = from;
    s.span = span;
  }

  //! The pass is over: each key it holds, it holds from its first new value
  //! to where the pass ended, all round for a recording.
  void end_pass() noexcept
  {
    if(!taking)
      return;
    edits++;
    for(int i = 0; i < touch_count; i++)
    {
      const auto& t = touches[i];
      if(t.stretch < 0)
        continue;
      auto& s = stretches[t.stretch];
      const int64_t span = whole ? length : pass_written - t.at;
      set_stretch(s, t, std::min(span, length));
    }
    // A stretch the pass did not get to is none.
    remove_material([](const auto& e) {
      if constexpr(requires { e.span; })
        return e.span <= 0;
      else
        return false;
    });
    touch_count = 0;
    taking = false;
  }

  //! Adds a value of the pass being taken in. A value on the frame of the
  //! last one of its key and kind in the pass takes its place.
  value_event* add_value(
      int64_t pos, int64_t written, uint16_t key, const ossia::value& v) noexcept
  {
    const bool impulse = v.get_type() == ossia::val_type::IMPULSE;
    const uint8_t pass = uint8_t(count - 1);
    for(int i = event_count - 1; i >= 0; i--)
    {
      auto& e = events[i];
      if(e.pass != pass || e.serial != serial)
        break;
      if(e.key == key && e.impulse == impulse)
      {
        if(e.pos != pos)
          break;
        e.value.set(v);
        edits++;
        return &e;
      }
    }
    if(event_count >= max_events)
    {
      full = true;
      return nullptr;
    }
    edits++;
    auto& e = events[event_count++];
    e.pos = pos;
    e.written = written;
    e.value.set(v);
    e.serial = serial;
    e.key = key;
    e.impulse = impulse;
    e.pass = pass;
    e.replaced_by = 0;
    e.merged = 0;
    return &e;
  }

  //! The pass being taken in went round the loop to [lo; hi[ again, `at`
  //! frames into it at lo: what it held there a round before is written over
  //! by what it holds now.
  void write_over(int64_t lo, int64_t hi, double at) noexcept
  {
    if(!taking || count == 0 || touch_count == 0 || at < double(length))
      return;
    const uint8_t pass = uint8_t(count - 1);
    remove_events([&](const value_event& e) {
      return e.pass == pass && e.serial == serial && !e.impulse && e.pos >= lo
             && e.pos < hi && touched(e.key)
             && double(e.written + length) <= at + double(e.pos - lo) + 1e-6;
    });
  }

  //! Copies what another loop holds, without its unused storage.
  void assign(const value_loop& o)
  {
    for(int i = 0; i < o.event_count; i++)
      events[i] = o.events[i];
    for(int i = o.event_count; i < event_count; i++)
      events[i].value.reset();
    std::copy_n(o.stretches.begin(), o.stretch_count, stretches.begin());
    event_count = o.event_count;
    stretch_count = o.stretch_count;
    full = o.full;
    next_serial = o.next_serial;
    touches = o.touches;
    touch_count = o.touch_count;
    serial = o.serial;
    whole = o.whole;
    assign_passes(o);
  }

  //! Whether a stretch holds position x, its pass repeating every `period`.
  static bool holds(const value_stretch& s, int64_t x, int64_t period) noexcept
  {
    if(s.span >= period)
      return true;
    int64_t d = (x - s.from) % period;
    if(d < 0)
      d += period;
    return d < s.span;
  }

  int64_t period_for(int pass) const noexcept
  {
    return std::clamp<int64_t>(period_of(pass), 1, std::max<int64_t>(1, length));
  }
};

//! Scratch space for working out what a loop holds: the values of a key,
//! by pass then position, and where what is held may change. Kept apart so
//! that it allocates nothing.
struct held_scratch
{
  static constexpr int max_refs = 2 * value_loop::max_events;
  struct ref
  {
    int64_t pos;
    uint32_t serial;
    int event;
  };
  std::array<ref, max_refs> refs{};
  int count{};
  std::array<int64_t, max_refs> cuts{};
  bool own_mute{true};

  static bool in(int pass, int first, int last) noexcept
  {
    return first < 0 || (pass >= first && pass <= last);
  }

  //! The values of `key` heard, each as often as its pass repeats. With
  //! `own_mute` false, a muted pass's as well.
  void prepare(const value_loop& loop, uint16_t key, bool own) noexcept
  {
    own_mute = own;
    count = 0;
    const int64_t len = loop.length;
    if(len <= 0)
      return;
    for(int i = 0; i < loop.event_count; i++)
    {
      const auto& e = loop.events[i];
      if(e.key != key || e.impulse || !loop.heard(e, own_mute))
        continue;
      const int64_t period = loop.period_for(e.pass);
      for(int64_t q = e.pos % period; q < len && count < max_refs; q += period)
        refs[count++] = {q, e.serial, i};
    }
    std::sort(refs.begin(), refs.begin() + count, [](const ref& a, const ref& b) {
      if(a.serial != b.serial)
        return a.serial < b.serial;
      if(a.pos != b.pos)
        return a.pos < b.pos;
      return a.event < b.event;
    });
  }

  //! The value of the loop that holds `key` at x, of the passes from `first`
  //! to `last`, -1 for all of them: that of the newest pass holding the key
  //! there, the last of it at or before x, round its stretch. -1 for none.
  int value_at(
      const value_loop& loop, uint16_t key, int64_t x, int first = -1,
      int last = -1) const noexcept
  {
    bool any = false;
    uint32_t top = 0;
    for(int s = 0; s < loop.stretch_count; s++)
    {
      const auto& st = loop.stretches[s];
      if(st.key != key || !in(st.pass, first, last) || !loop.heard(st, own_mute)
         || !value_loop::holds(st, x, loop.period_for(st.pass)))
        continue;
      if(!any || st.serial > top)
        top = st.serial;
      any = true;
    }
    if(!any)
      return -1;
    auto lo = std::lower_bound(
        refs.begin(), refs.begin() + count, top,
        [](const ref& r, uint32_t s) { return r.serial < s; });
    auto hi = std::upper_bound(
        lo, refs.begin() + count, top, [](uint32_t s, const ref& r) { return s < r.serial; });
    if(lo == hi)
      return -1;
    auto it = std::upper_bound(lo, hi, x, [](int64_t x, const ref& r) { return x < r.pos; });
    return it == lo ? std::prev(hi)->event : std::prev(it)->event;
  }
};

/**
 * What is heard of the held values of a key round a loop: the positions
 * where it changes, in order, and the value of the loop that holds from each
 * one until the next, the last holding past the loop's end into the first.
 * Worked out again from the loop each time what is heard may have changed.
 */
struct held_timeline
{
  static constexpr int max_points = held_scratch::max_refs;
  struct point
  {
    int64_t pos;
    //! The value of the loop that holds from there, -1 for none.
    int event;
  };
  std::array<point, max_points> points{};
  int count{};
  //! What it was worked out from.
  uint64_t edits{~uint64_t(0)};
  int64_t length{-1};

  //! The point that holds at x: the last one at or before it, round the
  //! loop. -1 when there are none.
  int at(int64_t x) const noexcept
  {
    if(count == 0)
      return -1;
    auto it = std::upper_bound(
        points.begin(), points.begin() + count, x,
        [](int64_t x, const point& p) { return x < p.pos; });
    const int i = int(it - points.begin()) - 1;
    return i >= 0 ? i : count - 1;
  }
  //! The point that holds just before x.
  int before(int64_t x) const noexcept { return at(x - 1); }
  int previous(int i) const noexcept { return i > 0 ? i - 1 : count - 1; }

  //! Works out what `loop` holds of `key`, of the passes from `first` to
  //! `last` (-1: all of them), unless nothing changed since. With `own_mute`
  //! false, a muted pass is heard.
  void update(
      const value_loop& loop, uint16_t key, held_scratch& scratch, int first = -1,
      int last = -1, bool own_mute = true, bool force = false) noexcept
  {
    if(!force && edits == loop.edits && length == loop.length)
      return;
    edits = loop.edits;
    length = loop.length;
    count = 0;
    const int64_t len = loop.length;
    if(len <= 0)
      return;
    scratch.prepare(loop, key, own_mute);

    // Where the held value may change: on a value, and where a stretch
    // starts or ends.
    auto& cuts = scratch.cuts;
    int c = 0;
    for(int i = 0; i < scratch.count && c < held_scratch::max_refs; i++)
      if(held_scratch::in(loop.events[scratch.refs[i].event].pass, first, last))
        cuts[c++] = scratch.refs[i].pos;
    for(int s = 0; s < loop.stretch_count; s++)
    {
      const auto& st = loop.stretches[s];
      if(st.key != key || !held_scratch::in(st.pass, first, last)
         || !loop.heard(st, own_mute))
        continue;
      const int64_t period = loop.period_for(st.pass);
      if(st.span >= period)
        continue;
      for(int64_t base = 0; base < len && c + 2 <= held_scratch::max_refs; base += period)
        for(const int64_t q : {base + st.from % period, base + (st.from + st.span) % period})
          if(q < len)
            cuts[c++] = q;
    }
    if(c == 0)
      return;
    if(c < held_scratch::max_refs)
      cuts[c++] = 0;
    std::sort(cuts.begin(), cuts.begin() + c);
    c = int(std::unique(cuts.begin(), cuts.begin() + c) - cuts.begin());

    for(int i = 0; i < c; i++)
    {
      const int e = scratch.value_at(loop, key, cuts[i], first, last);
      if(count > 0)
      {
        const int prev = points[count - 1].event;
        if(prev == e
           || (prev >= 0 && e >= 0 && loop.events[prev].value.same(loop.events[e].value)))
          continue;
      }
      points[count++] = {cuts[i], e};
    }
  }
};
}
