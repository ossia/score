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
  //! have taken them down to nothing.
  std::array<float, max_audio_layers> below_before{};
  std::array<float, max_audio_layers> below_after{};
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
 * channel. The memory is
 * reserved without being written, so only what is recorded into is actually
 * used.
 */
struct audio_layers
{
  static constexpr int max_layers = max_audio_layers;
  static constexpr int max_channels = 16;
  static constexpr int fade_frames = 128;
  //! Frames summed up into one point of the peak overview.
  static constexpr int peak_frames = 256;

  boost::container::vector<float> pool;
  boost::container::vector<float> peaks;
  int channels{};
  int slots{};
  int64_t capacity{};

  std::array<audio_layer, max_layers> layers{};
  //! Passes heard.
  int count{};
  //! Passes kept, the ones past `count` being what redo brings back.
  int total{};
  //! Frames in the loop.
  int64_t length{};

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
        std::size_t(channels) * std::size_t(slots) * std::size_t(capacity),
        boost::container::default_init);
    peaks.clear();
    peaks.shrink_to_fit();
    peaks.resize(std::size_t(slots) * peak_count(), 0.f);
    count = 0;
    total = 0;
    length = 0;
  }

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

  int64_t period_of(const audio_layer& l) const noexcept
  {
    return l.period > 0 ? l.period : length;
  }

  //! Where position k of the loop is in the layer's own buffer.
  int64_t index(const audio_layer& l, int64_t k) const noexcept
  {
    const int64_t p = period_of(l);
    return p > 0 ? k % p : 0;
  }

  bool valid(const audio_layer& l, int64_t k) const noexcept
  {
    const int64_t p = period_of(l);
    if(l.written >= p)
      return true;
    int64_t d = k % p - l.start;
    if(d < 0)
      d += p;
    return d < l.written;
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
    count = 1;
    total = 1;
    length = 0;
    layers[0] = audio_layer{.slot = 0, .gain = 1.f, .start = 0, .written = 0};
    std::fill_n(peaks_of(0), peak_count(), 0.f);
  }

  int free_slot() const noexcept
  {
    for(int s = 0; s < slots; s++)
    {
      bool used = false;
      for(int i = 0; i < count; i++)
        used |= layers[i].slot == s;
      if(!used)
        return s;
    }
    return -1;
  }

  //! Starts an overdub pass at position `start`. With every buffer taken, the
  //! two oldest passes are merged into one first, so the oldest undo is lost.
  void begin_overdub(int64_t start) noexcept
  {
    total = count;
    if(count >= slots)
      merge_oldest();
    const int slot = free_slot();
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

  //! Bakes the second pass and both gains into the first one.
  void merge_oldest() noexcept
  {
    if(count < 2)
      return;
    auto& a = layers[0];
    auto& b = layers[1];
    float* pa = peaks_of(a.slot);
    const float* pb = peaks_of(b.slot);
    // The merged pass is as long as the longer of the two: the shorter one is
    // repeated into it first.
    const int64_t pa_len = period_of(a), pb_len = period_of(b);
    const int64_t merged = std::max(pa_len, pb_len);
    for(int c = 0; c < channels; c++)
    {
      float* da = data(a.slot, c);
      const float* db = data(b.slot, c);
      for(int64_t k = pa_len; k < merged; k++)
        da[k] = da[k % pa_len];
      for(int64_t k = 0; k < merged; k++)
        da[k] = a.gain * da[k] + (valid(b, k) ? b.gain * db[k % pb_len] : 0.f);
    }
    a.period = merged == length ? 0 : merged;
    a.written = merged;
    {
      const std::size_t bins = std::size_t((merged + peak_frames - 1) / peak_frames);
      const std::size_t a_bins = std::max<std::size_t>(1, std::size_t((pa_len + peak_frames - 1) / peak_frames));
      for(std::size_t i = a_bins; i < bins; i++)
        pa[i] = pa[i % a_bins];
      for(std::size_t i = 0; i < bins; i++)
        pa[i] = a.gain * pa[i] + b.gain * pb[i];
    }
    a.gain = 1.f;
    for(int i = 1; i + 1 < count; i++)
      layers[i] = layers[i + 1];
    // The two oldest passes are one now, with their gains in it.
    for(int i = 1; i + 1 < count; i++)
    {
      for(auto* g : {&layers[i].below_before, &layers[i].below_after})
      {
        for(int j = 1; j + 1 < max_layers; j++)
          (*g)[j] = (*g)[j + 1];
        (*g)[0] = 1.f;
      }
    }
    count--;
    total = count;
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
