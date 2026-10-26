#pragma once
#include <Fx/Looper/Transport.hpp>

#include <boost/container/vector.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <utility>

namespace Nodes::Looper
{
//! One pass of the loop: the first recording, or an overdub on top of it.
//!
//! Its audio is kept apart from the other passes', so that undoing or erasing
//! it is hiding it and its gain can decay with feedback without touching what
//! it recorded.
inline constexpr int max_audio_layers = 32;

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
};

/**
 * The audio of every pass of a loop, in buffers reserved before playback.
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
struct audio_layers
{
  using pool_t = boost::container::vector<float>;

  static constexpr int max_layers = max_audio_layers;
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

  std::array<audio_layer, max_layers + 1> layers{};
  //! Passes heard.
  int count{};
  //! Passes kept, the ones past `count` being what redo brings back.
  int total{};
  //! Frames in the loop.
  int64_t length{};
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
    int64_t a_len{}, b_len{};
    int64_t merged{};
    int64_t pos{};
  } merging;

  //! The last pass merged into the newest one, and the newest one as it was:
  //! undo takes the pass out again. Their buffers are kept until something
  //! else changes the layers.
  struct unmerge_state
  {
    bool valid{};
    int at{};
    audio_layer a{};
    audio_layer b{};
  } unmerge;

  //! The layer the newest pass took the place of: undoing the pass puts it
  //! back where it was, redoing the pass takes it out again. Its buffer is
  //! kept until another pass starts or something else changes the layers.
  struct aside_state
  {
    bool valid{};
    //! Back in its place, the pass that replaced it being what redo brings
    //! back.
    bool returned{};
    int at{};
    //! The pass that replaced it.
    int by{};
    audio_layer layer{};
  } aside;

  merge_log merged_passes;

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

  //! The length a pass repeats with in a loop of `loop_length` frames.
  static int64_t period_in(const audio_layer& l, int64_t loop_length) noexcept
  {
    return l.period > 0 ? l.period : loop_length;
  }
  int64_t period_of(const audio_layer& l) const noexcept
  {
    return period_in(l, length);
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

  //! Mutes or unmutes pass k, k being heard.
  bool set_muted(int k, bool m) noexcept
  {
    if(k < 0 || k >= count || layers[k].muted == m)
      return false;
    layers[k].muted = m;
    // The merged pass is worked out with the mutes the two passes had.
    if(k == merging.at || k == merging.at + 1)
      merging.active = false;
    unmerge.valid = false;
    changes++;
    update_gains();
    return true;
  }

  //! What the loop sounds like at position k of channel c.
  float mix(int c, int64_t k) const noexcept
  {
    float sum = 0.f;
    for(int i = 0; i < count; i++)
    {
      const auto& l = layers[i];
      if(!l.muted && valid(l, k))
        sum += l.gain * data(l.slot, c)[index(l, k)];
    }
    return sum;
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
  bool extend(int64_t unit) noexcept
  {
    if(count == 0 || unit <= 0 || length + unit > capacity)
      return false;
    for(int i = 0; i < count - 1; i++)
      if(layers[i].period == 0)
        layers[i].period = length;
    length += unit;
    return true;
  }

  //! Forgets every pass and starts a recording in a free buffer.
  void begin_recording() noexcept
  {
    merging.active = false;
    unmerge.valid = false;
    aside.valid = false;
    count = 0;
    total = 0;
    length = 0;
    const int slot = std::max(0, take_slot());
    count = 1;
    total = 1;
    layers[0] = audio_layer{.slot = slot, .gain = 1.f, .decay = 1.f, .start = 0, .written = 0};
    std::fill_n(peaks_of(slot), peak_count(), 0.f);
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
        return s;
      if(fallback < 0)
        fallback = s;
    }
    if(fallback >= 0)
      reserved_overwritten = true;
    return fallback;
  }

  //! Starts an overdub pass at position `start`. With `slots` passes kept
  //! and `oldest` set, the two oldest start merging into one, so the oldest
  //! undo is lost; the new pass is heard at once, the merge going on in the
  //! following ticks. Without, the pass is one more, merged into the newest
  //! once it ends. `replace`, a pass heard, makes room for it instead: it is
  //! set aside, undo putting it back.
  void begin_overdub(int64_t start, bool oldest = true, int replace = -1) noexcept
  {
    total = count;
    unmerge.valid = false;
    aside.valid = false;
    if(replace >= 0)
      set_aside(replace);
    if(!oldest && count > slots)
    {
      // The pass before it was not merged yet.
      if(!merging.active)
        start_merge(count - 2, true);
      finish_merge();
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
    auto& l = layers[count];
    l.slot = slot;
    l.gain = 1.f;
    l.decay = 1.f;
    l.start = start;
    l.written = 0;
    l.period = 0;
    l.loop_before = length;
    l.loop_after = length;
    l.muted = false;
    l.backwards = false;
    std::fill_n(peaks_of(slot), peak_count(), 0.f);
    count++;
    total = count;
    aside.by = count - 1;
    update_gains();
  }

  //! Takes pass k out of the passes heard for a pass about to start, keeping
  //! it and its buffer for undo.
  void set_aside(int k) noexcept
  {
    if(k < 0 || k >= count || count < 2)
      return;
    if(k <= merging.at + 1)
      merging.active = false;
    aside = aside_state{.valid = true, .returned = false, .at = k, .by = -1, .layer = layers[k]};
    for(int i = k; i + 1 < total; i++)
      layers[i] = layers[i + 1];
    count--;
    total--;
    changes++;
    update_gains();
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
    if(total + 1 >= int(layers.size()))
    {
      aside.valid = false;
      return;
    }
    for(int i = total; i > aside.at; i--)
      layers[i] = layers[i - 1];
    layers[aside.at] = aside.layer;
    count++;
    total++;
    aside.returned = true;
  }

  //! Drops what redo would bring back.
  void drop_redo() noexcept
  {
    total = count;
    if(aside.returned)
      aside.valid = false;
  }

  audio_layer* top() noexcept { return count > 0 ? &layers[count - 1] : nullptr; }

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
    // A muted pass is left out of the merged one, and takes nothing from the
    // pass under it: merging is where it is lost.
    merging = merge_state{
        .active = true,
        .at = at,
        .target = target,
        .a_slot = a.slot,
        .b_slot = b.slot,
        .a_gain = a.muted ? 0.f : b.muted ? 1.f : b.decay,
        .b_gain = b.muted ? 0.f : 1.f,
        .muted = a.muted && b.muted,
        .undoable = undoable,
        .a = a,
        .b = b,
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
        dst[k] = (valid_in(m.a, k, m.a_len) ? m.a_gain * da[k % m.a_len] : 0.f)
                 + (valid_in(m.b, k, m.b_len) ? m.b_gain * db[k % m.b_len] : 0.f);
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

    if(m.undoable)
      unmerge = unmerge_state{.valid = true, .at = m.at, .a = m.a, .b = m.b};
    // What the two took from the passes under them, the merged one takes.
    auto& a = layers[m.at];
    a.slot = m.target;
    a.decay = (m.a.muted ? 1.f : m.a.decay) * (m.b.muted ? 1.f : m.b.decay);
    a.start = 0;
    a.period = m.merged == length ? 0 : m.merged;
    a.written = m.merged;
    a.loop_after = m.b.loop_after;
    a.muted = m.muted;
    a.backwards = false;
    for(int i = m.at + 1; i + 1 < count; i++)
      layers[i] = layers[i + 1];
    count--;
    total = count;
    m.active = false;
    aside.valid = false;
    merged_passes.add(m.at);
    changes++;
    update_gains();
  }

  void finish_merge() noexcept
  {
    merge_step(merging.merged);
  }

  //! Applies feedback to every pass under the top one, as one overdub pass
  //! goes round the loop. A muted pass takes nothing, but keeps what it
  //! would take for when it is heard again.
  void decay(float feedback) noexcept
  {
    if(count < 2 || feedback >= 1.f)
      return;
    layers[count - 1].decay *= feedback;
    update_gains();
  }

  bool undo() noexcept
  {
    if(count == 0)
      return false;
    merging.active = false;
    // The pass merged into the newest one comes out of it.
    if(std::exchange(unmerge.valid, false) && unmerge.at == count - 1)
    {
      layers[unmerge.at] = unmerge.a;
      layers[unmerge.at + 1] = unmerge.b;
      layers[unmerge.at + 1].loop_after = length;
      length = unmerge.b.loop_before;
      total = count + 1;
      changes++;
      update_gains();
      return true;
    }
    const bool back = returns_on_undo() >= 0;
    auto& l = layers[count - 1];
    if(count > 1)
    {
      l.loop_after = length;
      length = l.loop_before;
    }
    count--;
    if(back)
    {
      put_back();
      changes++;
    }
    update_gains();
    return true;
  }

  bool redo() noexcept
  {
    if(count >= total)
      return false;
    merging.active = false;
    unmerge.valid = false;
    // The pass that replaced a layer takes its place again.
    if(leaves_on_redo() >= 0)
    {
      aside.layer = layers[aside.at];
      for(int i = aside.at; i + 1 < total; i++)
        layers[i] = layers[i + 1];
      count--;
      total--;
      aside.returned = false;
      changes++;
    }
    const auto& l = layers[count];
    if(count > 0)
      length = l.loop_after;
    count++;
    update_gains();
    return true;
  }

  //! Takes pass k out of the loop, k being heard and not the only one heard:
  //! the passes over it and the ones redo brings back move down a place, the
  //! passes under it get back what its feedback took, and the loop keeps its
  //! length. Its buffer is free for a new pass.
  bool erase(int k) noexcept
  {
    if(k < 0 || k >= count || count < 2)
      return false;
    // The merged pass would not be made of the two passes any more.
    if(k <= merging.at + 1)
      merging.active = false;
    unmerge.valid = false;
    aside.valid = false;
    for(int i = k; i + 1 < total; i++)
      layers[i] = layers[i + 1];
    count--;
    total--;
    changes++;
    update_gains();
    return true;
  }

  void clear() noexcept
  {
    merging.active = false;
    unmerge.valid = false;
    aside.valid = false;
    count = 0;
    total = 0;
    length = 0;
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
    for(int64_t k = from; k < to; k++)
      if(audio_layers::valid_in(l, k, len))
        dst[k] += l.gain * src[audio_layers::index_in(l, k, len)];
  }
}

//! The passes of a loop as they were, and the memory they are in: what a
//! take written by a worker is flattened from, there, the looper being free
//! to be gone by then. Made on the main thread; the processing thread fills
//! it only while no job has it.
struct audio_snapshot
{
  std::shared_ptr<audio_layers::pool_t> pool;
  std::array<audio_layer, audio_layers::max_layers + 1> passes{};
  int count{};
  int channels{};
  int64_t capacity{};
  int64_t length{};

  //! Worker thread: the mix, `channels` channels into `out(c)`. The pool is
  //! let go of there, so that the processing thread never frees it.
  template <typename Out>
  void flatten(int out_channels, Out&& out)
  {
    if(pool)
    {
      auto& p = *pool;
      const auto data = [&](int slot, int c) -> const float* {
        return audio_layers::data_in(p, channels, capacity, slot, c);
      };
      for(int c = 0; c < std::min(out_channels, channels); c++)
        mix_passes(data, passes.data(), count, c, length, 0, length, out(c));
    }
    pool.reset();
  }
};
}
