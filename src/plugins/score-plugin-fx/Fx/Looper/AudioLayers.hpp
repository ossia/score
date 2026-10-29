#pragma once
#include <Fx/Looper/PassStack.hpp>

#include <boost/container/vector.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <utility>

namespace Nodes::Looper
{
inline constexpr int max_audio_layers = 32;

//! Where a pass meets, across a cut, the pass it went on from (its head) or
//! the one that went on from it (its tail): what was played there goes on
//! from one into the other, so neither is faded while the other is heard.
struct audio_seam_end
{
  //! The pass on the other side is not there yet.
  static constexpr int pending = -2;

  //! Index in the pass's buffer of its frame next to the seam, -1 for none.
  int64_t at{-1};
  //! For a tail: loop position of the other side's frame next to the seam.
  int64_t meet{-1};
  //! Buffer of the pass on the other side, which a merge forwards to the
  //! buffer of the merged pass; -1 for none.
  int partner{-1};
  //! Frames from `at` on, going `dir` into the pass, that the seam weighs.
  int n{};
  int dir{1};
  //! Weight given to the frame next to the seam, the next ones going to one
  //! across the n frames: one as recorded, zero faded.
  float rho{1.f};
};

//! One pass of the loop: the first recording, or an overdub on top of it.
//!
//! Its audio is kept apart from the other passes', so that undoing or erasing
//! it is hiding it and its gain can decay with feedback without touching what
//! it recorded.
struct audio_layer
{
  //! Which buffer of the pool holds it.
  int slot{-1};
  //! Gain it is played with: the product of the decays of the passes heard
  //! over it, zero while it is muted. Worked out by audio_layers::update_gains().
  float gain{1.f};
  //! The gain it would be played with were it not muted: what it is written
  //! with in a take of layers.
  float unmuted_gain{1.f};
  //! What its feedback took from every pass under it while it was the top
  //! one: the product of the feedback of each time it went round the loop.
  //! Hiding the pass, by undo or erase, gives it back to them.
  float decay{1.f};
  //! Loop position of its first frame.
  int64_t start{};
  //! Frames it was written for. Positions past them are silence until the
  //! pass has gone round the whole loop once.
  int64_t written{};
  //! Length it repeats with: zero while it is the length of the loop. A pass
  //! made before the loop was extended plays again over the longer loop.
  int64_t period{};
  //! Length of the loop before this pass, which undoing it gives back, and
  //! after it, which redoing it puts back.
  int64_t loop_before{};
  int64_t loop_after{};
  //! Kept, but not heard: as if erased for what the loop sounds like, its
  //! feedback included.
  bool muted{};
  //! The playhead went backwards over it last: its last frames are the first
  //! of the part it wrote.
  bool backwards{};
  //! Gain it was heard with when the gains last started to glide.
  float from{1.f};
  audio_seam_end head, tail;
};

/**
 * The audio of every pass of a loop, in buffers reserved before playback.
 * The passes are a pass_stack; this is what the audio of each is.
 *
 * `slots` buffers hold the passes, each `capacity` frames long for each
 * channel, and `spare_slots` more are kept for merging: past `slots` passes,
 * two layers are merged into a spare buffer a part per tick: the two oldest
 * while the overdub that asked for room goes into the other one, or the
 * newest and a pass that just ended over it. The memory is reserved without
 * being written, so only what is recorded into is actually used. It is
 * shared, so that a take of it written once the looper is gone still reads
 * it.
 */
struct audio_layers : pass_stack<audio_layers, audio_layer, max_audio_layers>
{
  using pool_t = boost::container::vector<float>;

  static constexpr int spare_slots = 2;
  static constexpr int max_channels = 16;
  static constexpr int fade_frames = 128;
  //! Frames summed up into one point of the peak overview.
  static constexpr int peak_frames = 256;

  std::shared_ptr<pool_t> pool;
  boost::container::vector<float> peaks;
  int channels{};
  int slots{};
  int64_t capacity{};

  //! Counts the passes erased, so that what shows them knows it is not
  //! looking at the same ones although there are as many.
  uint64_t changes{};

  //! Buffers something else still reads, e.g. a take being written: a new
  //! pass goes into another one when there is one.
  uint64_t reserved{};
  //! Buffers a take written from the looper's stop reads: kept from new
  //! passes while there are others.
  uint64_t held{};
  //! Set when a new pass had to go into a reserved buffer after all.
  bool reserved_overwritten{};

  //! Two passes heard one over the other, `at` and the one over it, being
  //! merged into a spare buffer.
  //! The samples of the ends of passes at a seam as recorded, for each
  //! buffer, end and channel: what the buffer holds there without weight
  //! (orig), and the part of it that the pass's own end is (part), which is
  //! all of it but in a merged pass. Allocated with the buffers.
  pool_t kept;
  //! The buffer each buffer was merged into, -1 for none: a seam's partner
  //! is the merged pass, until the merge is undone.
  std::array<int, max_layers + spare_slots> merged_into{};

  //! Frame counter of the processing: gains glide over frames of it.
  int64_t now{};
  //! Gains glide from `from` to `gain` until ramp_end, over ramp_len frames.
  int64_t ramp_end{};
  int64_t ramp_len{1};

  struct merge_state
  {
    bool active{};
    int at{};
    int target{-1};
    int a_slot{-1}, b_slot{-1};
    //! Gain of the lower pass relative to the upper one: the decay of the
    //! upper one. The merged pass is played with the gain of the upper one.
    float a_gain{};
    float b_gain{1.f};
    bool muted{};
    //! Kept for undo once merged: the pass merged into the newest one.
    bool undoable{};
    audio_layer a{};
    audio_layer b{};
    //! A muted pass merged with one that is not is lost from the merged
    //! one.
    bool lost_a{}, lost_b{};
    int64_t a_len{}, b_len{};
    int64_t merged{};
    int64_t pos{};
  } merging;

  //! Main thread only.
  void allocate(int chans, int layer_count, int64_t frames)
  {
    chans = std::clamp(chans, 1, max_channels);
    // One buffer for the recording and one at least for an overdub.
    layer_count = std::clamp(layer_count, 2, max_layers);
    frames = std::max<int64_t>(frames, 1);
    if(chans == channels && layer_count == slots && frames == capacity)
      return;

    channels = chans;
    slots = layer_count;
    capacity = frames;
    // A take still being written keeps the old memory.
    pool = std::make_shared<pool_t>(
        std::size_t(channels) * std::size_t(buffers()) * std::size_t(capacity),
        boost::container::default_init);
    peaks.clear();
    peaks.shrink_to_fit();
    peaks.resize(std::size_t(buffers()) * peak_count(), 0.f);
    kept.clear();
    kept.shrink_to_fit();
    kept.resize(std::size_t(buffers()) * 4 * std::size_t(channels) * fade_frames, 0.f);
    merged_into.fill(-1);
    ramp_end = 0;
    count = 0;
    total = 0;
    length = 0;
    reserved = 0;
    held = 0;
    merging = {};
    unmerge = {};
    aside = {};
    merged_passes = {};
  }

  // pass_stack's hooks: the buffers of the passes move with them, and a
  // merge going on is abandoned when the passes it merges move.
  void passes_changed() noexcept
  {
    changes++;
    update_gains();
  }
  bool may_remove(int) const noexcept { return true; }
  void muting(int k) noexcept
  {
    // The merged pass is worked out with the mutes the two passes had.
    if(k == merging.at || k == merging.at + 1)
      merging.active = false;
  }
  void removing(int k) noexcept
  {
    if(k <= merging.at + 1)
      merging.active = false;
  }
  void erased(int) noexcept { }
  void aside_taken(int) noexcept { }
  void aside_returned(int) noexcept { }
  void aside_forgotten() noexcept { }
  void redo_dropped(bool) noexcept { }
  void unmerge_dropped() noexcept { }
  void unmerged(int) noexcept
  {
    for(int s : {unmerge.a.slot, unmerge.b.slot})
      if(s >= 0 && s < buffers())
        merged_into[s] = -1;
  }
  void cleared() noexcept
  {
    merging.active = false;
    merged_into.fill(-1);
  }
  void undoing() noexcept { merging.active = false; }

  int buffers() const noexcept { return slots + spare_slots; }

  bool allocated() const noexcept { return capacity > 0 && pool && !pool->empty(); }
  bool empty() const noexcept { return count == 0 || length == 0; }
  std::size_t peak_count() const noexcept
  {
    return std::size_t((capacity + peak_frames - 1) / peak_frames);
  }

  //! Channel `chan` of buffer `slot` of a pool of `channels` channels of
  //! `capacity` frames a buffer.
  static float*
  data_in(pool_t& p, int channels, int64_t capacity, int slot, int chan) noexcept
  {
    return p.data()
           + (std::size_t(slot) * std::size_t(channels) + std::size_t(chan))
                 * std::size_t(capacity);
  }
  float* data(int slot, int chan) noexcept
  {
    return data_in(*pool, channels, capacity, slot, chan);
  }
  const float* data(int slot, int chan) const noexcept
  {
    return const_cast<audio_layers*>(this)->data(slot, chan);
  }
  float* peaks_of(int slot) noexcept
  {
    return peaks.data() + std::size_t(slot) * peak_count();
  }

  //! Where position k of a loop of `loop_length` frames is in the layer's own
  //! buffer, and whether the pass was written there.
  static int64_t index_in(const audio_layer& l, int64_t k, int64_t loop_length) noexcept
  {
    const int64_t p = period_in(l, loop_length);
    return p > 0 ? k % p : 0;
  }
  static bool valid_in(const audio_layer& l, int64_t k, int64_t loop_length) noexcept
  {
    const int64_t p = period_in(l, loop_length);
    if(l.written >= p)
      return true;
    int64_t d = k % p - l.start;
    if(d < 0)
      d += p;
    return d < l.written;
  }

  int64_t index(const audio_layer& l, int64_t k) const noexcept
  {
    return index_in(l, k, length);
  }
  bool valid(const audio_layer& l, int64_t k) const noexcept
  {
    return valid_in(l, k, length);
  }

  //! Gives every pass heard its gain: the decays of the passes heard over
  //! it. Called whenever a decay or the passes heard change.
  void update_gains() noexcept
  {
    float g = 1.f;
    for(int i = count - 1; i >= 0; i--)
    {
      auto& l = layers[i];
      l.unmuted_gain = g;
      l.gain = l.muted ? 0.f : g;
      if(!l.muted)
        g *= l.decay;
    }
  }

  //! Gain pass l is heard with at frame `now`.
  float heard_gain(const audio_layer& l) const noexcept
  {
    if(now >= ramp_end)
      return l.gain;
    return l.gain + (l.from - l.gain) * float(ramp_end - now) / float(ramp_len);
  }

  //! What the loop sounds like at position k of channel c.
  float mix(int c, int64_t k) const noexcept
  {
    float sum = 0.f;
    for(int i = 0; i < count; i++)
    {
      const auto& l = layers[i];
      if(l.muted || !valid(l, k))
        continue;
      const int64_t p = period_of(l);
      float v = data(l.slot, c)[k % p];
      if(p < length)
        v *= repeat_weight(l, k, p, length);
      sum += heard_gain(l) * v;
    }
    return sum;
  }

  //! Weight of frame d of an end of n frames at a seam, d = 0 being the one
  //! next to it: `rho` there, one past the end.
  static float seam_weight(bool tail, float rho, int64_t d, int n) noexcept
  {
    const float f = float(tail ? d + 1 : d) / float(n);
    return rho + (1.f - rho) * f;
  }

  //! A pass shorter than the loop plays again over the rest of it: its ends
  //! at a seam meet the other side only where they were played, and are
  //! faded wherever else they are heard. What position k of a loop of `len`
  //! frames is weighed with, `p` being the pass's period.
  static float
  repeat_weight(const audio_layer& l, int64_t k, int64_t p, int64_t len) noexcept
  {
    float w = 1.f;
    for(const bool tail : {false, true})
    {
      const auto& e = tail ? l.tail : l.head;
      if(e.at < 0 || e.n <= 0 || e.rho <= 0.f || (tail && e.meet < 0))
        continue;
      int64_t d = ((k % p - e.at) * e.dir) % p;
      if(d < 0)
        d += p;
      if(d >= e.n)
        continue;
      int64_t seam = (k - e.dir * d) % len;
      if(seam < 0)
        seam += len;
      int64_t met = tail ? (e.meet + e.dir) % len : e.at;
      if(met < 0)
        met += len;
      if(seam != met)
        w *= seam_weight(tail, 0.f, d, e.n) / seam_weight(tail, e.rho, d, e.n);
    }
    return w;
  }

  //! Position k of the loop was written by the pass: the part it wrote, which
  //! the playhead goes over without a gap in either direction, takes it in. A
  //! playhead that jumped leaves silence between that part and k.
  void cover(audio_layer& l, int64_t k) noexcept
  {
    const int64_t p = period_of(l);
    if(p <= 0 || l.written >= p)
      return;
    if(l.written == 0)
    {
      l.start = k;
      l.written = 1;
      return;
    }
    int64_t d = (k - l.start) % p;
    if(d < 0)
      d += p;
    if(d < l.written)
      return;
    const int64_t after = d - l.written;
    const int64_t before = p - 1 - d;
    if(after <= before)
    {
      silence(l, l.start + l.written, after);
      l.written = d + 1;
    }
    else
    {
      silence(l, k + 1, before);
      l.start = k;
      l.written += before + 1;
    }
    l.written = std::min(l.written, p);
  }

  //! Zeroes n positions of a pass from position `from` on, round its period.
  void silence(audio_layer& l, int64_t from, int64_t n) noexcept
  {
    const int64_t p = period_of(l);
    if(p <= 0 || n <= 0)
      return;
    for(int c = 0; c < channels; c++)
    {
      float* d = data(l.slot, c);
      for(int64_t i = 0; i < n; i++)
        d[(from + i) % p] = 0.f;
    }
  }

  //! The top pass goes on past the end of the loop: the loop grows by `unit`
  //! frames, and the passes under it play again over the new part. False when
  //! the buffers are full.
  bool extend(int64_t unit) noexcept { return pass_stack::extend(unit, capacity); }

  //! Forgets every pass and starts a recording in a free buffer.
  void begin_recording() noexcept
  {
    auto& l = begin_first_pass();
    l.slot = std::max(0, take_slot());
    std::fill_n(peaks_of(l.slot), peak_count(), 0.f);
  }

  bool slot_used(int s) const noexcept
  {
    if(merging.active && merging.target == s)
      return true;
    if(unmerge.valid && (unmerge.a.slot == s || unmerge.b.slot == s))
      return true;
    if(aside.valid && !aside.returned && aside.layer.slot == s)
      return true;
    for(int i = 0; i < count; i++)
      if(layers[i].slot == s)
        return true;
    return false;
  }

  bool is_reserved(int s) const noexcept { return ((reserved | held) >> s) & 1u; }

  //! A buffer no pass uses, one nothing else reads when there is one.
  int take_slot() noexcept
  {
    int fallback = -1;
    for(int s = 0; s < buffers(); s++)
    {
      if(slot_used(s))
        continue;
      if(!is_reserved(s))
      {
        buffer_reused(s);
        return s;
      }
      if(fallback < 0)
        fallback = s;
    }
    if(fallback >= 0)
    {
      reserved_overwritten = true;
      buffer_reused(fallback);
    }
    return fallback;
  }

  //! Every pass kept: heard, brought back by redo, set aside or merged.
  template <typename F>
  void for_each_kept(F&& f) noexcept
  {
    for(int i = 0; i < total; i++)
      f(layers[i]);
    if(aside.valid)
      f(aside.layer);
    if(unmerge.valid)
    {
      f(unmerge.a);
      f(unmerge.b);
    }
  }

  //! The buffer a merge forwarded buffer `s` to.
  int resolve(int s) const noexcept
  {
    for(int i = 0; s >= 0 && s < buffers() && merged_into[s] >= 0 && i < buffers(); i++)
      s = merged_into[s];
    return s;
  }

  //! Index of the pass heard in buffer `s`, or the buffer it was merged
  //! into; -1 for none.
  int heard_in(int s) const noexcept
  {
    s = resolve(s);
    if(s < 0)
      return -1;
    for(int i = 0; i < count; i++)
      if(layers[i].slot == s)
        return layers[i].muted ? -1 : i;
    return -1;
  }

  //! Buffer `s` takes a new pass: the one it held is gone for good, and the
  //! seams that met it meet nothing now.
  void buffer_reused(int s) noexcept
  {
    for_each_kept([&](audio_layer& l) {
      for(auto* e : {&l.head, &l.tail})
      {
        if(e->partner == s)
          e->partner = merged_into[s];
        else if(e->partner >= 0 && resolve(e->partner) == s)
          e->partner = -1;
      }
    });
    for(auto& m : merged_into)
      if(m == s)
        m = -1;
    merged_into[s] = -1;
  }

  float* kept_of(int slot, bool tail, bool part, int c) noexcept
  {
    return kept.data()
           + ((std::size_t(slot) * 4 + (tail ? 2 : 0) + (part ? 1 : 0)) * std::size_t(channels)
              + std::size_t(c))
                 * fade_frames;
  }

  //! Where frame d of an end of pass l is in its buffer.
  int64_t seam_index(const audio_layer& l, const audio_seam_end& e, int64_t d) const noexcept
  {
    const int64_t p = period_of(l);
    int64_t k = (e.at + e.dir * d) % p;
    return k < 0 ? k + p : k;
  }

  //! The first frames (head) or last frames (tail) pass l wrote meet a seam
  //! with the pass in buffer `partner`: they are kept as they are now, as
  //! recorded, to be weighed from.
  void open_seam(audio_layer& l, bool tail, int partner) noexcept
  {
    const int64_t p = period_of(l);
    const int64_t written = std::min(l.written, p);
    auto& e = tail ? l.tail : l.head;
    e = {};
    if(p <= 0 || written <= 0 || !allocated() || kept.empty())
      return;
    const int64_t first = l.start % p;
    const int64_t last = (l.start + written - 1) % p;
    // Backwards, the part written grows towards the loop's start: its first
    // frame is its last position.
    const bool at_last = tail != l.backwards;
    e.at = at_last ? last : first;
    e.dir = at_last ? -1 : 1;
    e.n = int(std::min<int64_t>(fade_frames, written));
    e.partner = partner;
    e.rho = 1.f;
    for(int c = 0; c < channels; c++)
    {
      const float* d = data(l.slot, c);
      float* orig = kept_of(l.slot, tail, false, c);
      float* part = kept_of(l.slot, tail, true, c);
      for(int i = 0; i < e.n; i++)
        orig[i] = part[i] = d[seam_index(l, e, i)];
    }
  }

  //! Writes an end of pass l with weight `rho` next to its seam.
  void weigh_seam(audio_layer& l, bool tail, float rho) noexcept
  {
    auto& e = tail ? l.tail : l.head;
    e.rho = rho;
    for(int c = 0; c < channels; c++)
    {
      float* d = data(l.slot, c);
      const float* orig = kept_of(l.slot, tail, false, c);
      const float* part = kept_of(l.slot, tail, true, c);
      for(int i = 0; i < e.n; i++)
        d[seam_index(l, e, i)] = orig[i] - part[i] * (1.f - seam_weight(tail, rho, i, e.n));
    }
  }

  //! Gain a pass is heard with once the gains stop gliding, or the one they
  //! glide from: a seam is weighed for what is heard over it, which only
  //! changes once the playhead has left it.
  float settled_gain(const audio_layer& l) const noexcept
  {
    return now < ramp_end ? l.from : l.gain;
  }

  //! Each end of a pass heard at a seam is faded when the other side is not
  //! heard, as recorded when it is; the louder side starts at the level of
  //! the other, so that a pass with feedback over the one it went on from
  //! goes on from it at its level.
  void update_seams() noexcept
  {
    for(int i = 0; i < count; i++)
    {
      auto& l = layers[i];
      if(l.muted)
        continue;
      for(const bool tail : {false, true})
      {
        auto& e = tail ? l.tail : l.head;
        if(e.at < 0 || e.partner == audio_seam_end::pending)
          continue;
        const int j = e.partner >= 0 ? heard_in(e.partner) : -1;
        if(j == i)
          continue;
        float rho = 0.f;
        if(j >= 0)
        {
          const float mine = settled_gain(l);
          const float theirs = settled_gain(layers[j]);
          rho = mine > 0.f ? std::min(1.f, theirs / mine) : 1.f;
          if(tail && layers[j].head.at >= 0)
            e.meet = layers[j].head.at;
        }
        if(std::abs(rho - e.rho) > 1e-6f)
          weigh_seam(l, tail, rho);
      }
    }
  }

  //! Starts an overdub pass at position `start`. With `slots` passes kept
  //! and `oldest` set, the two oldest start merging into one, so the oldest
  //! undo is lost; the new pass is heard at once, the merge going on in the
  //! following ticks. Without, the pass is one more, merged into the newest
  //! once it ends. `replace`, a pass heard, makes room for it instead: it is
  //! set aside, undo putting it back.
  void begin_overdub(int64_t start, bool oldest = true, int replace = -1) noexcept
  {
    drop_redo();
    forget_aside();
    if(replace >= 0)
      set_aside(replace);
    if(!oldest && count > slots)
    {
      // The pass before it was not merged yet. Once merged, it can no more
      // be taken out than when its merge ended before this pass: the two
      // buffers kept for that are free for this one.
      if(!merging.active)
        start_merge(count - 2, true);
      finish_merge();
      drop_unmerge();
    }
    if(oldest && count >= slots && !merging.active)
      start_merge(0);
    // No buffer left for the pass, e.g. one more overdub while the last merge
    // is not done: it is finished at once, which makes room.
    if(take_slot_count() == 0)
    {
      finish_merge();
      if(oldest && count >= slots)
        start_merge(0);
    }
    const int slot = take_slot();
    if(slot < 0)
      return;
    auto& l = push_pass();
    l.slot = slot;
    l.start = start;
    std::fill_n(peaks_of(slot), peak_count(), 0.f);
  }

  int take_slot_count() const noexcept
  {
    int n = 0;
    for(int s = 0; s < buffers(); s++)
      n += !slot_used(s);
    return n;
  }

  //! Starts merging pass `at` and the one over it into a spare buffer, the
  //! lower one scaled by the decay of the upper one, so that the merged pass
  //! is heard as the two were with the gain of the upper one. The two passes
  //! are heard as they are until the merged one takes their place. `undoable`
  //! keeps them for undo once merged.
  void start_merge(int at = 0, bool undoable = false) noexcept
  {
    if(at < 0 || at + 1 >= count || merging.active)
      return;
    const int target = take_slot();
    if(target < 0)
      return;
    const auto& a = layers[at];
    const auto& b = layers[at + 1];
    // A muted pass merged with one that is not is left out of the merged one,
    // and takes nothing from the pass under it: merging is where it is lost.
    // Two muted passes make a muted one that has both.
    const bool lost_a = a.muted && !b.muted, lost_b = b.muted && !a.muted;
    merging = merge_state{
        .active = true,
        .at = at,
        .target = target,
        .a_slot = a.slot,
        .b_slot = b.slot,
        .a_gain = lost_a ? 0.f : lost_b ? 1.f : b.decay,
        .b_gain = lost_b ? 0.f : 1.f,
        .muted = a.muted && b.muted,
        .undoable = undoable,
        .a = a,
        .b = b,
        .lost_a = lost_a,
        .lost_b = lost_b,
        .a_len = std::max<int64_t>(1, period_of(a)),
        .b_len = std::max<int64_t>(1, period_of(b)),
        .merged = std::max(period_of(a), period_of(b)),
        .pos = 0};
  }

  //! Merges up to `frames` more frames; the merged pass takes the place of
  //! the two once they are all done. Abandoned when the passes it merges are
  //! no longer where they were.
  void merge_step(int64_t frames) noexcept
  {
    auto& m = merging;
    if(!m.active)
      return;
    if(m.at + 1 >= count || layers[m.at].slot != m.a_slot
       || layers[m.at + 1].slot != m.b_slot)
    {
      m.active = false;
      return;
    }

    // The merged pass is as long as the longer of the two: the shorter one
    // repeats into it.
    const int64_t end = std::min(m.merged, m.pos + std::max<int64_t>(1, frames));
    for(int c = 0; c < channels; c++)
    {
      float* dst = data(m.target, c);
      const float* da = data(m.a_slot, c);
      const float* db = data(m.b_slot, c);
      for(int64_t k = m.pos; k < end; k++)
        dst[k] = merged_at(da, db, k);
    }
    {
      float* pt = peaks_of(m.target);
      const float* pa = peaks_of(m.a_slot);
      const float* pb = peaks_of(m.b_slot);
      const int64_t a_bins = std::max<int64_t>(1, (m.a_len + peak_frames - 1) / peak_frames);
      const int64_t b_bins = std::max<int64_t>(1, (m.b_len + peak_frames - 1) / peak_frames);
      for(int64_t i = m.pos / peak_frames; i < (end + peak_frames - 1) / peak_frames; i++)
        pt[i] = m.a_gain * pa[i % a_bins] + m.b_gain * pb[i % b_bins];
    }
    m.pos = end;
    if(m.pos < m.merged)
      return;

    m.active = false;
    begin_merge();
    // What the two took from the passes under them, the merged one takes.
    audio_layer a = merged_pass(layers[m.at], layers[m.at + 1]);
    merge_seams(a);
    a.from = layers[m.at + 1].from;
    a.slot = m.target;
    a.decay = (m.lost_a ? 1.f : m.a.decay) * (m.lost_b ? 1.f : m.b.decay);
    a.start = 0;
    a.period = m.merged == length ? 0 : m.merged;
    a.written = m.merged;
    a.muted = m.muted;
    a.backwards = false;
    merged(m.at, a, m.undoable, m.a, m.b);
  }

  void finish_merge() noexcept
  {
    merge_step(merging.merged);
  }

  //! Position k of the merged pass, from the buffers of the two.
  float merged_at(const float* da, const float* db, int64_t k) const noexcept
  {
    const auto& m = merging;
    const auto& a = layers[m.at];
    const auto& b = layers[m.at + 1];
    float va = 0.f, vb = 0.f;
    if(valid_in(m.a, k, m.a_len))
    {
      va = da[k % m.a_len];
      if(m.a_len < m.merged)
        va *= repeat_weight(a, k, m.a_len, length);
    }
    if(valid_in(m.b, k, m.b_len))
    {
      vb = db[k % m.b_len];
      if(m.b_len < m.merged)
        vb *= repeat_weight(b, k, m.b_len, length);
    }
    return m.a_gain * va + m.b_gain * vb;
  }

  //! The seams of the two passes merged into `into`, which takes their ends
  //! at a seam with another pass: the lower pass's head, the upper one's
  //! tail. Where they meet each other is in it as it is heard now.
  void merge_seams(audio_layer& into) noexcept
  {
    auto& m = merging;
    const auto& a = layers[m.at];
    const auto& b = layers[m.at + 1];
    // The ends of the two are merged again from their buffers as they are
    // now: a seam may have been weighed while the merge went on.
    for(int c = 0; c < channels; c++)
    {
      float* dst = data(m.target, c);
      const float* da = data(m.a_slot, c);
      const float* db = data(m.b_slot, c);
      for(const auto* l : {&a, &b})
      {
        const int64_t p = std::max<int64_t>(1, period_of(*l));
        for(const auto* e : {&l->head, &l->tail})
          if(e->at >= 0)
            for(int i = 0; i < e->n; i++)
              for(int64_t k = seam_index(*l, *e, i); k < m.merged; k += p)
                dst[k] = merged_at(da, db, k);
      }
    }

    into.head = {};
    into.tail = {};
    const auto outer = [&](const audio_layer& l, bool tail, const audio_layer& other,
                           int64_t len, bool lost) {
      const auto& e = tail ? l.tail : l.head;
      return e.at >= 0 && !lost && len == m.merged
             && (e.partner < 0 || resolve(e.partner) != other.slot);
    };
    const auto take = [&](const audio_layer& l, bool tail, float gain) {
      const auto& e = tail ? l.tail : l.head;
      (tail ? into.tail : into.head) = e;
      for(int c = 0; c < channels; c++)
      {
        const float* dst = data(m.target, c);
        const float* part = kept_of(l.slot, tail, true, c);
        float* to_orig = kept_of(m.target, tail, false, c);
        float* to_part = kept_of(m.target, tail, true, c);
        for(int i = 0; i < e.n; i++)
        {
          to_part[i] = gain * part[i];
          to_orig[i] = dst[seam_index(l, e, i)]
                       + to_part[i] * (1.f - seam_weight(tail, e.rho, i, e.n));
        }
      }
    };
    // One end a side: another one there is kept in the merged pass as it is
    // heard now.
    if(outer(a, false, b, m.a_len, m.lost_a))
      take(a, false, m.a_gain);
    else if(outer(b, false, a, m.b_len, m.lost_b))
      take(b, false, m.b_gain);
    if(outer(b, true, a, m.b_len, m.lost_b))
      take(b, true, m.b_gain);
    else if(outer(a, true, b, m.a_len, m.lost_a))
      take(a, true, m.a_gain);

    merged_into[m.a_slot] = m.lost_a ? -1 : m.target;
    merged_into[m.b_slot] = m.lost_b ? -1 : m.target;
  }

  //! Applies feedback to every pass under the top one, as one overdub pass
  //! goes round the loop. A muted pass takes nothing, but keeps what it
  //! would take for when it is heard again. The gains glide to their new
  //! values over `glide` frames from `now`: a step would click.
  void decay(float feedback, int64_t glide = fade_frames) noexcept
  {
    if(count < 2 || feedback >= 1.f)
      return;
    for(int i = 0; i < count; i++)
      layers[i].from = heard_gain(layers[i]);
    layers[count - 1].decay *= feedback;
    update_gains();
    ramp_len = std::max<int64_t>(1, glide);
    ramp_end = now + ramp_len;
  }

  void note_peak(int slot, int64_t k, float v) noexcept
  {
    float& p = peaks_of(slot)[std::size_t(k / peak_frames)];
    p = std::max(p, std::abs(v));
  }

  //! The positions of the top pass that were written last, faded to silence
  //! so that the end of a punch does not click.
  void fade_out_top() noexcept
  {
    auto* l = top();
    if(!l || length <= 0 || l->written <= 0)
      return;
    const int64_t p = period_of(*l);
    const int64_t n = std::min<int64_t>({fade_frames, l->written, p});
    for(int c = 0; c < channels; c++)
    {
      float* d = data(l->slot, c);
      for(int64_t i = 0; i < n; i++)
      {
        int64_t k = l->backwards ? (l->start + i) % p
                                 : (l->start + std::min(l->written, p) - 1 - i) % p;
        if(k < 0)
          k += p;
        d[k] *= float(i) / float(n);
      }
    }
  }

  //! Moves a pass earlier in the loop by `frames`: what was recorded at
  //! position p belongs at p - frames, the input having reached the looper
  //! that late.
  void shift_earlier(audio_layer& l, int64_t frames) noexcept
  {
    const int64_t len = period_of(l);
    if(len <= 0)
      return;
    // A negative count moves it later: the input of a pass the playhead went
    // over backwards belongs further on in the loop.
    frames %= len;
    if(frames < 0)
      frames += len;
    if(frames <= 0)
      return;
    for(int c = 0; c < channels; c++)
    {
      float* d = data(l.slot, c);
      std::rotate(d, d + frames, d + len);
    }
    float* p = peaks_of(l.slot);
    const int64_t bins = (len + peak_frames - 1) / peak_frames;
    const int64_t bin_shift = frames / peak_frames;
    if(bin_shift > 0 && bins > 0)
      std::rotate(p, p + (bin_shift % bins), p + bins);
    l.start -= frames;
    if(l.start < 0)
      l.start += len;
    for(auto* e : {&l.head, &l.tail})
      if(e->at >= 0)
        e->at = ((e->at - frames) % len + len) % len;
  }
};

//! Frames [from; to[ of channel c of what is heard of `count` passes of a
//! loop of `len` frames, into dst; `data(slot, c)` is where a buffer is.
template <typename Data>
void mix_passes(
    Data&& data, const audio_layer* passes, int count, int c, int64_t len, int64_t from,
    int64_t to, float* dst)
{
  std::fill(dst + from, dst + to, 0.f);
  for(int i = 0; i < count; i++)
  {
    const auto& l = passes[i];
    const float* src = data(l.slot, c);
    const int64_t p = audio_layers::period_in(l, len);
    for(int64_t k = from; k < to; k++)
      if(audio_layers::valid_in(l, k, len))
        dst[k] += l.gain * src[audio_layers::index_in(l, k, len)]
                  * (p < len ? audio_layers::repeat_weight(l, k, p, len) : 1.f);
  }
}

//! The passes of a loop as they were, and the memory they are in: what a
//! take written by a worker is flattened from, there, the looper being free
//! to be gone by then. Made on the main thread; the processing thread fills
//! it only while no job has it.
struct audio_snapshot
{
  std::shared_ptr<audio_layers::pool_t> pool;
  std::array<audio_layer, audio_layers::max_passes> passes{};
  int count{};
  int channels{};
  int64_t capacity{};
  int64_t length{};

  //! Worker thread: what is heard of the passes (item -1), or of pass
  //! `item` alone, as a muted pass would be heard, `channels` channels into
  //! `out(c)`.
  template <typename Out>
  void flatten(int out_channels, Out&& out, int item = -1) const
  {
    if(!pool)
      return;
    auto& p = *pool;
    const auto data = [&](int slot, int c) -> const float* {
      return audio_layers::data_in(p, channels, capacity, slot, c);
    };
    for(int c = 0; c < std::min(out_channels, channels); c++)
    {
      float* dst = out(c);
      if(item < 0)
      {
        mix_passes(data, passes.data(), count, c, length, 0, length, dst);
        continue;
      }
      const auto& l = passes[item];
      const float* src = data(l.slot, c);
      const int64_t p = audio_layers::period_in(l, length);
      for(int64_t k = 0; k < length; k++)
        dst[k] = audio_layers::valid_in(l, k, length)
                     ? l.unmuted_gain * src[audio_layers::index_in(l, k, length)]
                           * (p < length ? audio_layers::repeat_weight(l, k, p, length) : 1.f)
                     : 0.f;
    }
  }
};
}
