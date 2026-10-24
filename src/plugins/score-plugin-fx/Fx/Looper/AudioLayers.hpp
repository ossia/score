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
struct audio_layer
{
  //! Which buffer of the pool holds it.
  int slot{-1};
  //! Gain it is played with: one, times the feedback of every overdub pass
  //! that went over it since.
  float gain{1.f};
  //! Gain this pass applied to the passes under it, so that undoing it can
  //! give it back.
  float applied{1.f};
  //! Loop position of its first frame.
  int64_t start{};
  //! Frames it was written for. Positions past them are silence until the
  //! pass has gone round the whole loop once.
  int64_t written{};
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
  static constexpr int max_layers = 32;
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
    layer_count = std::clamp(layer_count, 1, max_layers);
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

  bool valid(const audio_layer& l, int64_t k) const noexcept
  {
    if(l.written >= length)
      return true;
    int64_t d = k - l.start;
    if(d < 0)
      d += length;
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
        sum += l.gain * data(l.slot, c)[k];
    }
    return sum;
  }

  //! Forgets every pass and starts a recording in the first buffer.
  void begin_recording() noexcept
  {
    count = 1;
    total = 1;
    length = 0;
    layers[0] = audio_layer{.slot = 0, .gain = 1.f, .applied = 1.f, .start = 0, .written = 0};
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
    layers[count] = audio_layer{.slot = slot, .gain = 1.f, .applied = 1.f, .start = start, .written = 0};
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
    for(int c = 0; c < channels; c++)
    {
      float* da = data(a.slot, c);
      const float* db = data(b.slot, c);
      for(int64_t k = 0; k < length; k++)
        da[k] = a.gain * da[k] + (valid(b, k) ? b.gain * db[k] : 0.f);
    }
    for(std::size_t i = 0; i < peak_count(); i++)
      pa[i] = a.gain * pa[i] + b.gain * pb[i];
    a.gain = 1.f;
    for(int i = 1; i + 1 < count; i++)
      layers[i] = layers[i + 1];
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
    layers[count - 1].applied *= feedback;
  }

  bool undo() noexcept
  {
    if(count == 0)
      return false;
    const float applied = layers[count - 1].applied;
    if(applied > 1e-6f)
      for(int i = 0; i < count - 1; i++)
        layers[i].gain /= applied;
    count--;
    return true;
  }

  bool redo() noexcept
  {
    if(count >= total)
      return false;
    const float applied = layers[count].applied;
    for(int i = 0; i < count; i++)
      layers[i].gain *= applied;
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
    const int64_t n = std::min<int64_t>({fade_frames, l->written, length});
    for(int c = 0; c < channels; c++)
    {
      float* d = data(l->slot, c);
      for(int64_t i = 0; i < n; i++)
      {
        int64_t k = (l->start + std::min(l->written, length) - 1 - i) % length;
        if(k < 0)
          k += length;
        d[k] *= float(i) / float(n);
      }
    }
  }

  //! Moves a pass earlier in the loop by `frames`: what was recorded at
  //! position p belongs at p - frames, the input having reached the looper
  //! that late.
  void shift_earlier(audio_layer& l, int64_t frames) noexcept
  {
    if(length <= 0)
      return;
    frames %= length;
    if(frames <= 0)
      return;
    for(int c = 0; c < channels; c++)
    {
      float* d = data(l.slot, c);
      std::rotate(d, d + frames, d + length);
    }
    float* p = peaks_of(l.slot);
    const int64_t bins = (length + peak_frames - 1) / peak_frames;
    const int64_t bin_shift = frames / peak_frames;
    if(bin_shift > 0 && bins > 0)
      std::rotate(p, p + (bin_shift % bins), p + bins);
    l.start -= frames;
    if(l.start < 0)
      l.start += length;
  }
};
}
