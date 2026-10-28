#pragma once
#include <Fx/Looper/Transport.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <utility>

namespace Nodes::Looper
{
//! What a loop whose material is kept apart keeps of a pass besides it.
struct tagged_pass
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
 * The passes of a loop, one over the other, whatever they hold.
 *
 * A pass is the first recording or an overdub on top of it. Undo hides the
 * newest one and redo shows it again; erase takes one out for good; mute
 * keeps one but does not hear it; a pass past Max layers is merged with
 * another, or takes the place of one, which is set aside for undo to put
 * back. The stack does the bookkeeping of all of it, the same for every
 * looper: which passes are heard and which redo brings back, the length each
 * one repeats with, the length of the loop before and after each one, which
 * ones are muted, the layer set aside and the merge undo can take apart.
 *
 * `Pass` holds, besides what the looper keeps of its own for a pass (a
 * buffer, a gain...), `period`, `loop_before`, `loop_after` and `muted`, and
 * is copied whole as the passes move. What a pass recorded may instead be
 * kept apart, each piece tagged with the index of its pass and with the
 * passes that replace it, e.g. MIDI events: `Store` moves those along as the
 * passes move, through these hooks, which tagged_material below implements
 * for any such piece:
 *
 *   void passes_changed();          // what is heard of the passes changed
 *   bool may_remove(int k) const;   // pass k can be erased or set aside now
 *   void muting(int k);             // pass k is about to be muted or unmuted
 *   void removing(int k);           // pass k is about to be erased or set
 *                                   // aside: the passes over it move down
 *   void erased(int k);             // pass k is erased: its material goes
 *   void aside_taken(int k);        // pass k goes to the side
 *   void aside_returned(int k);     // the pass set aside comes back at k
 *   void aside_forgotten();         // the pass set aside is gone for good
 *   void redo_dropped(bool aside);  // passes from `count` on are gone for
 *                                   // good, the one set aside kept if `aside`
 *   void unmerge_dropped();         // the last merge cannot be undone now
 *   void unmerged(int at);          // the last merge, of `at` and the pass
 *                                   // over it, is undone
 *   void cleared();                 // every pass is gone
 *   void undoing();                 // undo or redo is about to act
 */
template <typename Store, typename Pass, int MaxLayers>
struct pass_stack
{
  //! Most layers kept: Max layers is at most this.
  static constexpr int max_layers = MaxLayers;
  //! A pass past Max layers is one more until it is merged.
  static constexpr int max_passes = max_layers + 1;

  //! The passes heard, then the ones redo brings back.
  std::array<Pass, max_passes + 1> layers{};
  //! Passes heard.
  int count{};
  //! Passes kept, the ones past `count` being what redo brings back.
  int total{};
  //! Frames in the loop.
  int64_t length{};
  //! Counts the passes added over the ones heard: whether a pass asked for
  //! got a place.
  uint64_t pushes{};

  //! The last pass merged into the newest one, and the two as they were:
  //! undo takes the pass out again, as long as nothing else changes the
  //! passes.
  struct unmerge_state
  {
    bool valid{};
    int at{};
    Pass a{};
    Pass b{};
  } unmerge;

  //! The layer the newest pass took the place of: undoing the pass puts it
  //! back where it was, redoing the pass takes it out again. It is kept until
  //! another pass starts or something else changes the passes.
  struct aside_state
  {
    bool valid{};
    //! Back in its place, the pass that replaced it being what redo brings
    //! back.
    bool returned{};
    int at{};
    //! The pass that replaced it.
    int by{-1};
    Pass layer{};
  } aside;

  merge_log merged_passes;

  bool empty() const noexcept { return count == 0 || length == 0; }

  Pass* top() noexcept { return count > 0 ? &layers[count - 1] : nullptr; }

  //! The length a pass repeats with in a loop of `loop_length` frames.
  static int64_t period_in(const Pass& l, int64_t loop_length) noexcept
  {
    return l.period > 0 ? l.period : loop_length;
  }
  int64_t period_of(const Pass& l) const noexcept { return period_in(l, length); }
  int64_t period_of(int pass) const noexcept { return period_in(layers[pass], length); }

  //! The period two passes merged into one repeat with.
  int64_t merged_period(const Pass& a, const Pass& b) const noexcept
  {
    const int64_t p = std::max(period_of(a), period_of(b));
    return p == length ? 0 : p;
  }

  //! The top pass goes on past the end of the loop: the loop grows by `unit`
  //! frames, and the passes under it play again over the new part. False when
  //! the loop would be longer than `max_length`. Without `top_open`, the pass
  //! going on is not in the stack yet: every pass plays again.
  bool extend(int64_t unit, int64_t max_length, bool top_open = true) noexcept
  {
    if(count == 0 || unit <= 0 || length + unit > max_length)
      return false;
    for(int i = 0; i < count - (top_open ? 1 : 0); i++)
      if(layers[i].period == 0)
        layers[i].period = length;
    length += unit;
    return true;
  }

  //! Mutes or unmutes pass k, k being heard.
  bool set_muted(int k, bool m) noexcept
  {
    if(k < 0 || k >= count || layers[k].muted == m)
      return false;
    self().muting(k);
    layers[k].muted = m;
    drop_unmerge();
    self().passes_changed();
    return true;
  }

  void clear() noexcept
  {
    unmerge.valid = false;
    aside.valid = false;
    count = 0;
    total = 0;
    length = 0;
    self().cleared();
    self().passes_changed();
  }

  //! Forgets every pass for a recording: its pass, the first one.
  Pass& begin_first_pass() noexcept
  {
    clear();
    count = 1;
    total = 1;
    layers[0] = Pass{};
    return layers[0];
  }

  //! Adds a pass over the ones heard, room having been made for it.
  Pass& push_pass() noexcept
  {
    auto& l = layers[count];
    l = Pass{};
    l.loop_before = length;
    l.loop_after = length;
    count++;
    total = count;
    pushes++;
    aside.by = count - 1;
    self().passes_changed();
    return l;
  }

  //! Drops what redo would bring back. A layer set aside is kept, for undo,
  //! unless it is back.
  void drop_redo() noexcept
  {
    const bool keep_aside = aside.valid && !aside.returned;
    self().redo_dropped(keep_aside);
    total = count;
    if(!keep_aside)
      aside.valid = false;
    drop_unmerge();
    self().passes_changed();
  }

  //! Forgets the layer set aside.
  void forget_aside() noexcept
  {
    if(!aside.valid)
      return;
    aside.valid = false;
    if(!aside.returned)
      self().aside_forgotten();
  }

  //! Undo is not to take the last merge apart: what it kept for that goes.
  void drop_unmerge() noexcept
  {
    if(std::exchange(unmerge.valid, false))
      self().unmerge_dropped();
  }

  //! Takes pass k out of the passes heard for a pass about to start, keeping
  //! it for undo: the passes over it move down a place.
  void set_aside(int k) noexcept
  {
    if(k < 0 || k >= count || count < 2 || !self().may_remove(k))
      return;
    forget_aside();
    drop_unmerge();
    move_aside(k);
    aside.by = -1;
  }

  //! Undoing the top pass puts back the layer it replaced: where it goes.
  int returns_on_undo() const noexcept
  {
    return aside.valid && !aside.returned && count > 0 && aside.by == count - 1 ? aside.at
                                                                                 : -1;
  }

  //! Redoing the pass that replaced a layer takes that layer out again: the
  //! one that goes.
  int leaves_on_redo() const noexcept
  {
    return aside.valid && aside.returned && count + 1 == total && aside.at < count ? aside.at
                                                                                 : -1;
  }

  //! The layer set aside goes back to its place, the passes from there on,
  //! the ones redo brings back included, moving up a place.
  void put_back() noexcept
  {
    const int k = aside.at;
    if(total >= max_passes)
    {
      forget_aside();
      return;
    }
    self().aside_returned(k);
    for(int i = total; i > k; i--)
      layers[i] = layers[i - 1];
    layers[k] = aside.layer;
    count++;
    total++;
    aside.returned = true;
    self().passes_changed();
  }

  bool undo() noexcept
  {
    if(count == 0)
      return false;
    self().undoing();
    // The pass merged into the newest one comes out of it.
    if(unmerge.valid && unmerge.at == count - 1)
    {
      forget_aside();
      const int at = unmerge.at;
      unmerge.valid = false;
      self().unmerged(at);
      layers[at] = unmerge.a;
      layers[at + 1] = unmerge.b;
      layers[at + 1].loop_after = length;
      length = unmerge.b.loop_before;
      total = count + 1;
      self().passes_changed();
      return true;
    }
    drop_unmerge();
    const bool back = returns_on_undo() >= 0;
    auto& l = layers[count - 1];
    if(count > 1)
    {
      l.loop_after = length;
      length = l.loop_before;
    }
    count--;
    if(back)
      put_back();
    self().passes_changed();
    return true;
  }

  bool redo() noexcept
  {
    if(count >= total)
      return false;
    self().undoing();
    drop_unmerge();
    // The pass that replaced a layer takes its place again.
    if(const int k = leaves_on_redo(); k >= 0)
    {
      const int by = aside.by;
      move_aside(k);
      aside.by = by;
    }
    if(count > 0)
      length = layers[count].loop_after;
    count++;
    self().passes_changed();
    return true;
  }

  //! Takes pass k out of the loop, k being heard and not the only one heard:
  //! the passes over it and the ones redo brings back move down a place, and
  //! the loop keeps its length.
  bool erase(int k) noexcept
  {
    if(k < 0 || k >= count || count < 2 || !self().may_remove(k))
      return false;
    drop_unmerge();
    forget_aside();
    self().removing(k);
    self().erased(k);
    shift_down(k);
    self().passes_changed();
    return true;
  }

  //! A merge is about to be worked out: what it would make stale goes.
  void begin_merge() noexcept
  {
    if(total > count)
      drop_redo();
    drop_unmerge();
    forget_aside();
  }

  //! Pass `at` and the one over it, `a` and `b` as they were, are now one
  //! pass, `merged`, in the place of `at`. With `undoable`, undo takes `b`
  //! out again as long as nothing else changes the passes.
  void merged(int at, const Pass& merged, bool undoable, const Pass& a, const Pass& b) noexcept
  {
    if(undoable)
      unmerge = unmerge_state{.valid = true, .at = at, .a = a, .b = b};
    layers[at] = merged;
    for(int i = at + 1; i + 1 < count; i++)
      layers[i] = layers[i + 1];
    count--;
    total = count;
    merged_passes.add(at);
    self().passes_changed();
  }

  //! The pass `a` and `b` heard one over the other become, before what the
  //! looper keeps of its own for it: a muted pass merged with one that is
  //! not is lost, and both muted, the merged pass is muted.
  Pass merged_pass(const Pass& a, const Pass& b) const noexcept
  {
    Pass m = a;
    m.period = merged_period(a, b);
    m.loop_after = b.loop_after;
    m.muted = a.muted && b.muted;
    return m;
  }

  //! One of the passes of `by` replaces material of pass `self` now: it is
  //! heard, not muted, and not the pass being taken in when `taking`, which
  //! replaces as it goes. The material's own pass, for a pass made of two
  //! merged ones, replaces it whatever.
  bool replaced(uint64_t by, int self_pass, bool taking) const noexcept
  {
    for(uint64_t m = by; m != 0; m &= m - 1)
    {
      const int q = std::countr_zero(m);
      if(q == self_pass)
        return true;
      if(q < count && !layers[q].muted && !(taking && q == count - 1))
        return true;
    }
    return false;
  }

private:
  Store& self() noexcept { return static_cast<Store&>(*this); }

  void shift_down(int k) noexcept
  {
    for(int i = k; i + 1 < total; i++)
      layers[i] = layers[i + 1];
    count--;
    total--;
  }

  void move_aside(int k) noexcept
  {
    self().removing(k);
    aside.valid = true;
    aside.returned = false;
    aside.at = k;
    aside.layer = layers[k];
    self().aside_taken(k);
    shift_down(k);
    self().passes_changed();
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

//! What became of a piece of material when the last pass was merged into the
//! newest one, for undo to take it apart again.
enum merge_mark : uint8_t
{
  from_upper = 1,        ///< It is of the upper pass.
  replaced_by_lower = 2, ///< The lower pass replaced it.
  replaced_by_upper = 4  ///< The upper pass replaced it.
};

/**
 * Material kept apart from the passes, each piece tagged with `pass`, the
 * index of its pass, `replaced_by`, the passes that replace it, a bit each,
 * and `merged`, its merge_marks: how a pass_stack's hooks move it along.
 *
 * Two indices past every pass there can be hold the material of the layer
 * set aside, and that of a muted pass a merge lost, which undoing the merge
 * gives back.
 */
template <int MaxPasses>
struct tagged_material
{
  static constexpr int aside_pass = MaxPasses;
  static constexpr int lost_pass = MaxPasses + 1;
  static_assert(lost_pass < 64);
  static constexpr uint64_t bit(int k) noexcept { return uint64_t(1) << k; }

  template <typename T>
  static void erase(T& e, int k) noexcept
  {
    e.replaced_by = without_pass(e.replaced_by, k);
    if(e.pass > k)
      e.pass--;
  }

  template <typename T>
  static void set_aside(T& e, int k) noexcept
  {
    const bool by_k = e.replaced_by & bit(k);
    e.replaced_by = without_pass(e.replaced_by, k);
    if(by_k)
      e.replaced_by |= bit(aside_pass);
    if(e.pass == k)
      e.pass = uint8_t(aside_pass);
    else if(e.pass > k)
      e.pass--;
  }

  template <typename T>
  static void put_back(T& e, int k) noexcept
  {
    const bool by_aside = e.replaced_by & bit(aside_pass);
    e.replaced_by = with_room_at(e.replaced_by & ~bit(aside_pass), k);
    if(by_aside)
      e.replaced_by |= bit(k);
    if(e.pass == aside_pass)
      e.pass = uint8_t(k);
    else if(e.pass >= k)
      e.pass++;
  }

  //! Passes `a` and `a + 1` become one. What either of them replaces, the
  //! merged one does, its own material under it included, which stays
  //! silent; but a lost pass, muted and merged with one that is not, takes
  //! its material and its replacing with it.
  template <typename T>
  static void merge(T& e, int a, bool lost_a, bool lost_b) noexcept
  {
    const int b = a + 1;
    const bool had_a = e.replaced_by & bit(a);
    const bool had_b = e.replaced_by & bit(b);
    e.merged = uint8_t(
        (e.pass == b ? from_upper : 0) | (had_a ? replaced_by_lower : 0)
        | (had_b ? replaced_by_upper : 0));
    uint64_t r = e.replaced_by & ~(bit(a) | bit(b));
    if((had_a && !lost_a) || (had_b && !lost_b))
      r |= bit(a);
    e.replaced_by = without_pass(r, b);
    if((e.pass == a && lost_a) || (e.pass == b && lost_b))
      e.pass = uint8_t(lost_pass);
    else if(e.pass >= b && e.pass < aside_pass)
      e.pass--;
  }

  //! The merge of `a` and `a + 1` is undone.
  template <typename T>
  static void unmerge(T& e, int a) noexcept
  {
    const int b = a + 1;
    uint64_t r = with_room_at(e.replaced_by, b) & ~bit(a);
    if(e.merged & replaced_by_lower)
      r |= bit(a);
    if(e.merged & replaced_by_upper)
      r |= bit(b);
    e.replaced_by = r;
    if(e.pass == lost_pass || (e.merged & from_upper))
      e.pass = uint8_t((e.merged & from_upper) ? b : a);
    e.merged = 0;
  }
};

/**
 * A pass_stack whose material is all kept apart and tagged as
 * tagged_material says: the hooks that move it along with its passes, the
 * merging of two passes, and where the pass being taken in went. `Store`
 * gives, besides the pass_stack hooks it wants to do more in:
 *
 *   template <typename F> void for_each_material(F&& f);   // every piece
 *   template <typename F> void remove_material(F&& gone);  // those `gone` says
 *   void clear_material();       // every pass is gone
 *   void pass_state_begun();     // a pass starts: what it touched is forgotten
 */
template <typename Store, typename Pass, int MaxLayers>
struct tagged_pass_stack : pass_stack<Store, Pass, MaxLayers>
{
  using stack = pass_stack<Store, Pass, MaxLayers>;
  using tags = tagged_material<stack::max_passes>;
  //! Where the material of a pass set aside is, past every pass there can be.
  static constexpr int aside_pass = tags::aside_pass;

  //! Changes each time what is heard of the loop may have changed.
  uint64_t edits{};

  // The pass being taken in: where it went.
  int64_t pass_start{};
  int64_t pass_written{};
  //! The frames the pass went over, not rounded: at a rate other than one, a
  //! tick goes over a fraction of a frame more than its whole frames.
  double pass_progress{};
  //! The playhead goes backwards: the part the pass went over grows towards
  //! the loop's start, pass_start being its first position.
  bool pass_backwards{};
  bool taking{};

  // pass_stack's hooks: the material moves with its passes.
  void passes_changed() noexcept { edits++; }
  bool may_remove(int k) const noexcept { return !(taking && k == this->count - 1); }
  void muting(int) noexcept { }
  void removing(int) noexcept { }
  void erased(int k) noexcept
  {
    store().remove_material([k](const auto& e) { return e.pass == k; });
    store().for_each_material([k](auto& e) { tags::erase(e, k); });
  }
  void aside_taken(int k) noexcept
  {
    store().for_each_material([k](auto& e) { tags::set_aside(e, k); });
  }
  void aside_returned(int k) noexcept
  {
    store().for_each_material([k](auto& e) { tags::put_back(e, k); });
  }
  void aside_forgotten() noexcept
  {
    store().remove_material([](const auto& e) { return e.pass == aside_pass; });
    store().for_each_material([](auto& e) { e.replaced_by &= ~tags::bit(aside_pass); });
  }
  void redo_dropped(bool keep_aside) noexcept
  {
    const int heard_passes = this->count;
    store().remove_material([heard_passes, keep_aside](const auto& e) {
      return e.pass >= heard_passes && !(keep_aside && e.pass == aside_pass);
    });
    uint64_t kept
        = this->count >= 64 ? ~uint64_t(0) : (uint64_t(1) << this->count) - 1;
    if(keep_aside)
      kept |= tags::bit(aside_pass);
    store().for_each_material([kept](auto& e) { e.replaced_by &= kept; });
  }
  void unmerge_dropped() noexcept
  {
    store().remove_material([](const auto& e) { return e.pass == tags::lost_pass; });
  }
  void unmerged(int at) noexcept
  {
    store().for_each_material([at](auto& e) { tags::unmerge(e, at); });
  }
  void cleared() noexcept
  {
    taking = false;
    store().clear_material();
  }
  void undoing() noexcept { }

  //! Position k is in what the pass being taken in went over.
  bool in_pass(int64_t k) const noexcept
  {
    if(!taking)
      return false;
    if(pass_written >= this->length)
      return true;
    int64_t d = k - pass_start;
    if(d < 0)
      d += this->length;
    return d < pass_written;
  }

  //! The pass being taken in replaces what is under it as it goes, unless it
  //! is muted.
  bool replacing() const noexcept
  {
    return taking && this->count > 0 && !this->layers[this->count - 1].muted;
  }

  void begin_recording() noexcept
  {
    this->begin_first_pass();
    begin_pass_state(0);
  }

  //! Starts an overdub pass at `start`. With `layers` passes, the two oldest
  //! are merged first when `oldest` is set; else the pass is one more, merged
  //! into the newest when it ends. `replace`, a pass heard, makes room for it
  //! instead: it is set aside, undo putting it back.
  void begin_overdub(
      int64_t start, int max = stack::max_layers, bool oldest = true,
      int replace = -1) noexcept
  {
    this->drop_redo();
    this->forget_aside();
    if(replace >= 0)
      this->set_aside(replace);
    max = std::clamp(max, 2, stack::max_layers);
    if(oldest)
    {
      while(this->count >= max)
        merge(0);
    }
    else
    {
      // The pass before it was not merged yet.
      while(this->count > max)
        merge(this->count - 2);
    }
    this->push_pass();
    begin_pass_state(start);
  }

  //! Merges pass `at` and the one over it into one, heard as the two were.
  //! A muted pass merged with one that is not is left out of the merged one:
  //! merging is where it is lost. With `undoable`, undo takes the upper pass
  //! out again, as long as nothing else changes the passes.
  void merge(int at, bool undoable = false) noexcept
  {
    const int a = at, b = at + 1;
    if(a < 0 || b >= this->count)
      return;
    this->begin_merge();
    const Pass was_a = this->layers[a], was_b = this->layers[b];
    const bool lost_a = was_a.muted && !was_b.muted;
    const bool lost_b = was_b.muted && !was_a.muted;
    store().for_each_material([=](auto& e) { tags::merge(e, a, lost_a, lost_b); });
    this->merged(a, this->merged_pass(was_a, was_b), undoable, was_a, was_b);
  }

  void begin_pass_state(int64_t start) noexcept
  {
    pass_start = start;
    pass_written = 0;
    pass_progress = 0.;
    pass_backwards = false;
    taking = true;
    store().pass_state_begun();
  }

  //! The pass went over `frames` more frames of the loop, backwards when
  //! the playhead goes backwards.
  void advance_pass(double frames, bool backwards = false) noexcept
  {
    pass_progress += frames;
    const auto written = int64_t(std::floor(pass_progress + 1e-6));
    if(backwards && this->length > 0)
    {
      pass_start = (pass_start - (written - pass_written)) % this->length;
      if(pass_start < 0)
        pass_start += this->length;
    }
    pass_backwards = backwards;
    pass_written = written;
  }

  //! Copies the passes of another loop and where its pass went, not its
  //! material.
  void assign_passes(const tagged_pass_stack& o) noexcept
  {
    this->count = o.count;
    this->total = o.total;
    this->length = o.length;
    this->layers = o.layers;
    this->unmerge = o.unmerge;
    this->aside = o.aside;
    edits = o.edits;
    pass_start = o.pass_start;
    pass_written = o.pass_written;
    pass_progress = o.pass_progress;
    pass_backwards = o.pass_backwards;
    taking = o.taking;
  }

  //! Frames from `from` to `to` going forwards round the loop, a whole round
  //! for the same position.
  int64_t distance(int64_t from, int64_t to) const noexcept
  {
    if(this->length <= 0)
      return std::max<int64_t>(1, to - from);
    int64_t d = (to - from) % this->length;
    if(d <= 0)
      d += this->length;
    return d;
  }

private:
  Store& store() noexcept { return static_cast<Store&>(*this); }
};
}
