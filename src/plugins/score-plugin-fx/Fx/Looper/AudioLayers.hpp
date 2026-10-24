#pragma once
#include <boost/container/vector.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace Nodes::Looper
{
//! One pass of the loop: the first recording, or an overdub on top of it.
//!
//! Its audio is kept apart from the other passes', so that undoing it is
//! hiding it and its gain can decay with feedback without touching what it
//! recorded.
inline constexpr int max_audio_layers = 32;

struct audio_layer
{
  //! Which buffer of the pool holds it.
  int slot{-1};
  //! Gain it is played with: one, times the feedback of every overdub pass
  //! that went over it since.
  float gain{1.f};
  //! Gains of the passes under it when it started, which undoing it gives
  //! back, and when it was undone, which redoing it puts back: feedback may
  //! have taken them down to nothing. One more than the passes kept: an
  //! overdub goes on top while the two oldest are being merged.
  std::array<float, max_audio_layers + 1> below_before{};
  std::array<float, max_audio_layers + 1> below_after{};
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
};

/**
 * The audio of every pass of a loop, in buffers reserved before playback.
 *
 * `slots` buffers hold the passes, each `capacity` frames long for each
 * channel, and `spare_slots` more are kept for merging: past `slots` passes,
 * the two oldest are merged into a spare buffer a part per tick, while the
 * overdub that asked for room goes into the other one. The memory is
 * reserved without being written, so only what is recorded into is actually
 * used.
 */
struct audio_layers
{
  static constexpr int max_layers = max_audio_layers;
  static constexpr int spare_slots = 2;
  static constexpr int max_channels = 16;
  static constexpr int fade_frames = 128;
  //! Frames summed up into one point of the peak overview.
  static constexpr int peak_frames = 256;

  boost::container::vector<float> pool;
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

  //! Buffers something else still reads, e.g. a take being written: a new
  //! pass goes into another one when there is one.
  uint64_t reserved{};
  //! Set when a new pass had to go into a reserved buffer after all.
  bool reserved_overwritten{};

  //! The two oldest passes being merged into a spare buffer.
  struct merge_state
  {
    bool active{};
    int target{-1};
    int a_slot{-1}, b_slot{-1};
    //! Gains they are merged with: feedback taking both down meanwhile is
    //! put on the merged pass when it takes their place.
    float a_gain{}, b_gain{};
    audio_layer b{};
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
    pool.clear();
    pool.shrink_to_fit();
    pool.resize(
        std::size_t(channels) * std::size_t(buffers()) * std::size_t(capacity),
        boost::container::default_init);
    peaks.clear();
    peaks.shrink_to_fit();
    peaks.resize(std::size_t(buffers()) * peak_count(), 0.f);
    count = 0;
    total = 0;
    length = 0;
    reserved = 0;
    merging = {};
  }

  int buffers() const noexcept { return slots + spare_slots; }

  bool allocated() const noexcept { return capacity > 0 && !pool.empty(); }
  bool empty() const noexcept { return count == 0 || length == 0; }
  std::size_t peak_count() const noexcept
  {
    return std::size_t((capacity + peak_frames - 1) / peak_frames);
  }

  float* data(int slot, int chan) noexcept
  {
    return pool.data()
           + (std::size_t(slot) * std::size_t(channels) + std::size_t(chan))
                 * std::size_t(capacity);
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

  //! What the loop sounds like at position k of channel c.
  float mix(int c, int64_t k) const noexcept
  {
    float sum = 0.f;
    for(int i = 0; i < count; i++)
    {
      const auto& l = layers[i];
      if(valid(l, k))
        sum += l.gain * data(l.slot, c)[index(l, k)];
    }
    return sum;
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

  //! Forgets every pass and starts a recording in the first buffer.
  void begin_recording() noexcept
  {
    merging.active = false;
    count = 1;
    total = 1;
    length = 0;
    layers[0] = audio_layer{.slot = 0, .gain = 1.f, .start = 0, .written = 0};
    std::fill_n(peaks_of(0), peak_count(), 0.f);
  }

  bool slot_used(int s) const noexcept
  {
    if(merging.active && merging.target == s)
      return true;
    for(int i = 0; i < count; i++)
      if(layers[i].slot == s)
        return true;
    return false;
  }

  bool is_reserved(int s) const noexcept { return (reserved >> s) & 1u; }

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

  //! Starts an overdub pass at position `start`. With `slots` passes kept,
  //! the two oldest start merging into one, so the oldest undo is lost; the
  //! new pass is heard at once, the merge going on in the following ticks.
  void begin_overdub(int64_t start) noexcept
  {
    total = count;
    if(count >= slots && !merging.active)
      start_merge();
    // No buffer left for the pass, e.g. one more overdub while the last merge
    // is not done: it is finished at once, which makes room.
    if(take_slot_count() == 0)
    {
      finish_merge();
      if(count >= slots)
        start_merge();
    }
    const int slot = take_slot();
    if(slot < 0)
      return;
    auto& l = layers[count];
    l.slot = slot;
    l.gain = 1.f;
    l.start = start;
    l.written = 0;
    l.period = 0;
    l.loop_before = length;
    l.loop_after = length;
    for(int i = 0; i < count; i++)
      l.below_before[i] = layers[i].gain;
    std::fill_n(peaks_of(slot), peak_count(), 0.f);
    count++;
    total = count;
  }

  audio_layer* top() noexcept { return count > 0 ? &layers[count - 1] : nullptr; }

  int take_slot_count() const noexcept
  {
    int n = 0;
    for(int s = 0; s < buffers(); s++)
      n += !slot_used(s);
    return n;
  }

  //! Starts merging the second pass and both gains into a spare buffer. The
  //! two passes are heard as they are until the merged one takes their place.
  void start_merge() noexcept
  {
    if(count < 2 || merging.active)
      return;
    const int target = take_slot();
    if(target < 0)
      return;
    const auto& a = layers[0];
    const auto& b = layers[1];
    merging = merge_state{
        .active = true,
        .target = target,
        .a_slot = a.slot,
        .b_slot = b.slot,
        .a_gain = a.gain,
        .b_gain = b.gain,
        .b = b,
        .a_len = std::max<int64_t>(1, period_of(a)),
        .b_len = std::max<int64_t>(1, period_of(b)),
        .merged = std::max(period_of(a), period_of(b)),
        .pos = 0};
  }

  //! Merges up to `frames` more frames; the merged pass takes the place of
  //! the two oldest once they are all done. Abandoned when the passes it
  //! merges are no longer the two oldest.
  void merge_step(int64_t frames) noexcept
  {
    auto& m = merging;
    if(!m.active)
      return;
    if(count < 2 || layers[0].slot != m.a_slot || layers[1].slot != m.b_slot)
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
        dst[k] = m.a_gain * da[k % m.a_len]
                 + (valid_in(m.b, k, m.b_len) ? m.b_gain * db[k % m.b_len] : 0.f);
    }
    {
      float* pt = peaks_of(m.target);
      const float* pa = peaks_of(m.a_slot);
      const float* pb = peaks_of(m.b_slot);
      const int64_t a_bins = std::max<int64_t>(1, (m.a_len + peak_frames - 1) / peak_frames);
      for(int64_t i = m.pos / peak_frames; i < (end + peak_frames - 1) / peak_frames; i++)
        pt[i] = m.a_gain * pa[i % a_bins] + m.b_gain * pb[i];
    }
    m.pos = end;
    if(m.pos < m.merged)
      return;

    // Feedback takes every pass under the top one down alike, so what it took
    // from the two since the merge started is the same share of each.
    const float scale = m.a_gain > 0.f   ? layers[0].gain / m.a_gain
                        : m.b_gain > 0.f ? layers[1].gain / m.b_gain
                                         : 0.f;
    // A gain an upper pass gives back on undo or redo, as a gain of the
    // merged pass.
    auto merged_gain = [&](float ga, float gb) {
      return m.a_gain > 0.f ? ga / m.a_gain : m.b_gain > 0.f ? gb / m.b_gain : 0.f;
    };

    auto& a = layers[0];
    a.slot = m.target;
    a.gain = scale;
    a.start = 0;
    a.period = m.merged == length ? 0 : m.merged;
    a.written = m.merged;
    for(int i = 1; i + 1 < count; i++)
      layers[i] = layers[i + 1];
    for(int i = 1; i + 1 < count; i++)
    {
      for(auto* g : {&layers[i].below_before, &layers[i].below_after})
      {
        (*g)[0] = merged_gain((*g)[0], (*g)[1]);
        for(std::size_t j = 1; j + 1 < g->size(); j++)
          (*g)[j] = (*g)[j + 1];
      }
    }
    count--;
    total = count;
    m.active = false;
  }

  void finish_merge() noexcept
  {
    merge_step(merging.merged);
  }

  //! Applies feedback to every pass under the top one, as one overdub pass
  //! goes round the loop.
  void decay(float feedback) noexcept
  {
    if(count < 2 || feedback >= 1.f)
      return;
    for(int i = 0; i < count - 1; i++)
      layers[i].gain *= feedback;
  }

  bool undo() noexcept
  {
    if(count == 0)
      return false;
    merging.active = false;
    auto& l = layers[count - 1];
    for(int i = 0; i < count - 1; i++)
    {
      l.below_after[i] = layers[i].gain;
      layers[i].gain = l.below_before[i];
    }
    if(count > 1)
    {
      l.loop_after = length;
      length = l.loop_before;
    }
    count--;
    return true;
  }

  bool redo() noexcept
  {
    if(count >= total)
      return false;
    merging.active = false;
    const auto& l = layers[count];
    for(int i = 0; i < count; i++)
      layers[i].gain = l.below_after[i];
    if(count > 0)
      length = l.loop_after;
    count++;
    return true;
  }

  void clear() noexcept
  {
    merging.active = false;
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
        int64_t k = (l->start + std::min(l->written, p) - 1 - i) % p;
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
    frames %= len;
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
}
