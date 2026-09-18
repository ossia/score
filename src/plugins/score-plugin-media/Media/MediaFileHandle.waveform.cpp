#include <Media/MediaFileHandle.hpp>

#include <ossia/detail/libav.hpp>
#include <ossia/detail/ssize.hpp>

#include <algorithm>
#include <limits>
#include <vector>
namespace Media
{
namespace
{
template <typename Fun_T, typename T>
struct FrameComputer
{
  const int64_t start_frame;
  const int64_t end_frame;
  ossia::small_vector<T, 8>& sum;
  static constexpr const Fun_T fun{};

  void operator()(ossia::monostate) const noexcept { }

  void operator()(const AudioFile::StreamView& r) const noexcept
  {
    const int channels = r.handle->channels();
    assert(std::ssize(sum) == channels);
    if(end_frame - start_frame > 0)
    {
      bool init = false;
      r.handle->fetch(
          start_frame, end_frame - start_frame, [&](float* frame, float* end) {
            assert(frame < end);
            if(!init)
            {
              init = true;
              for(int c = 0; c < channels; c++)
              {
                assert(frame + c < end);
                sum[c] = fun.init(frame[c]);
              }
            }
            else
            {
              for(int c = 0; c < channels; c++)
              {
                assert(frame + c < end);
                auto& s = sum[c];
                s = fun(s, (float)frame[c]);
              }
            }
          });
    }
    else
    {
      r.handle->fetch(start_frame, 1, [&](float* frame, float* end) {
        assert(frame < end);
        for(int c = 0; c < channels; c++)
        {
          assert(frame + c < end);
          sum[c] = fun.init(frame[c]);
        }
      });
    }
  }
  void operator()(const AudioFile::RAMView& r) noexcept
  {
    const int channels = r.data.size();
    assert(std::ssize(sum) == channels);
    if(end_frame - start_frame > 0)
    {
      for(int c = 0; c < channels; c++)
      {
        const auto& vals = r.data[c];
        auto& s = sum[c];
        s = fun.init(vals[start_frame]);
        for(int64_t i = start_frame + 1; i < end_frame; i++)
          s = fun(s, (float)vals[i]);
      }
    }
    else if(end_frame == start_frame)
    {
      for(int c = 0; c < channels; c++)
      {
        const auto& vals = r.data[c];
        sum[c] = fun.init(vals[start_frame]);
      }
    }
  }

  void operator()(AudioFile::MmapView& r) noexcept
  {
    auto& wav = r.wav;
    const int channels = wav.channels();
    assert(std::ssize(sum) == channels);

    if(end_frame - start_frame > 0)
    {
      const int64_t buffer_size = end_frame - start_frame;
      thread_local std::vector<float> data_cache;

      if(Q_UNLIKELY(!wav.seek_to_pcm_frame(start_frame)))
        return;

      float* floats{};
      const int64_t num_elems = buffer_size * channels;
      if(num_elems > 10000)
      {
        data_cache.resize(num_elems);
        floats = data_cache.data();
      }
      else
      {
        floats = (float*)alloca(sizeof(float) * num_elems);
      }

      auto max = wav.read_pcm_frames_f32(buffer_size, floats);
      if(Q_UNLIKELY(max == 0))
        return;

      for(int c = 0; c < channels; c++)
      {
        sum[c] = fun.init(floats[c]);
      }

      for(decltype(max) i = 1; i < max; i++)
      {
        for(int c = 0; c < channels; c++)
        {
          const float f = floats[i * channels + c];
          sum[c] = fun(sum[c], f);
        }
      }
    }
    else
    {
      float* val = (float*)alloca(sizeof(float) * channels);
      if(Q_UNLIKELY(!wav.seek_to_pcm_frame(start_frame)))
        return;
      int max = wav.read_pcm_frames_f32(1, val);
      if(Q_UNLIKELY(max == 0))
        return;

      for(int c = 0; c < channels; c++)
      {
        sum[c] = fun.init(val[c]);
      }
    }
  }
};

struct MinMax
{
  static constexpr FloatPair init(float v) noexcept { return {v, v}; }
  static constexpr auto init(int64_t v) noexcept { return std::make_pair(v, v); }
  constexpr FloatPair operator()(FloatPair f1, float f2) const noexcept
  {
    return {f1.first < f2 ? f1.first : f2, f1.second > f2 ? f1.second : f2};
  }
  constexpr auto operator()(std::pair<int64_t, int64_t> f1, int64_t f2) const noexcept
  {
    return std::make_pair(f1.first < f2 ? f1.first : f2, f1.second > f2 ? f1.second : f2);
  }
};

//! Reduces the whole source into a summary's buckets.
//!
//! Reads straight through rather than asking for one bucket at a time: the
//! point is to touch every sample exactly once, and on the mmap path each read
//! is a seek plus a conversion to float.
struct SummaryBuilder
{
  WaveformSummary& s;
  bool& ok;

  void operator()(ossia::monostate) const noexcept { }

  //! A streamed source is not summarised: filling the table means decoding the
  //! entire stream, which is the thing streaming was chosen to avoid.
  void operator()(const AudioFile::StreamView&) const noexcept { }

  void operator()(const AudioFile::RAMView& r) const noexcept
  {
    if(std::ssize(r.data) < s.channels)
      return;

    for(int c = 0; c < s.channels; c++)
    {
      const audio_sample* const v = r.data[c];
      FloatPair* const dst = s.data.get() + int64_t(c) * s.bucketCount;
      for(int64_t b = 0; b < s.bucketCount; b++)
      {
        const int64_t beg = b * s.bucket;
        const int64_t end = std::min(beg + s.bucket, s.frames);
        float lo = v[beg], hi = v[beg];
        for(int64_t i = beg + 1; i < end; i++)
        {
          const float f = v[i];
          lo = f < lo ? f : lo;
          hi = f > hi ? f : hi;
        }
        dst[b] = {lo, hi};
      }
    }
    ok = true;
  }

  void operator()(AudioFile::MmapView& r) const noexcept
  {
    auto& wav = r.wav;
    const int channels = s.channels;
    if(wav.channels() != channels)
      return;

    // A megabyte per read, so the seek and the call overhead disappear against
    // the conversion. Budgeted in samples rather than frames: a file with many
    // channels would otherwise size this by its channel count.
    const int64_t chunk = std::clamp<int64_t>(
        (1 << 18) / (s.bucket * int64_t(channels)), 1, s.bucketCount);
    std::vector<float> buf(std::size_t(chunk * s.bucket * channels));

    if(!wav.seek_to_pcm_frame(0))
      return;

    for(int64_t b0 = 0; b0 < s.bucketCount; b0 += chunk)
    {
      const int64_t nb = std::min(chunk, s.bucketCount - b0);
      const int64_t got = wav.read_pcm_frames_f32(nb * s.bucket, buf.data());
      if(got <= 0)
        return;
      // Only the last chunk may come up short; anywhere else the reads that
      // follow would no longer line up with the buckets they fill.
      if(got < nb * s.bucket && b0 + nb < s.bucketCount)
        return;

      for(int64_t b = 0; b < nb; b++)
      {
        const int64_t beg = b * s.bucket;
        const int64_t end = std::min(beg + s.bucket, got);
        if(beg >= end)
          break;

        for(int c = 0; c < channels; c++)
        {
          float lo = buf[beg * channels + c], hi = lo;
          for(int64_t i = beg + 1; i < end; i++)
          {
            const float f = buf[i * channels + c];
            lo = f < lo ? f : lo;
            hi = f > hi ? f : hi;
          }
          s.data[int64_t(c) * s.bucketCount + b0 + b] = {lo, hi};
        }
      }
    }
    ok = true;
  }
};

struct SingleFrameComputer
{
  int64_t start_frame;
  ossia::small_vector<float, 8>& sum;

  void operator()(ossia::monostate) const noexcept { }

  void operator()(const AudioFile::StreamView& r) noexcept
  {
    const int channels = r.handle->channels();
    assert(std::ssize(sum) == channels);
    r.handle->fetch(start_frame, 1, [&](float* frame, float* end) {
      for(int c = 0; c < channels; c++)
      {
        assert(frame + c < end);
        sum[c] = frame[c];
      }
    });
  }
  void operator()(const AudioFile::RAMView& r) noexcept
  {
    const int channels = r.data.size();
    assert(std::ssize(sum) == channels);
    for(int c = 0; c < channels; c++)
    {
      const auto& vals = r.data[c];
      sum[c] = vals[start_frame];
    }
  }

  void operator()(AudioFile::MmapView& r) noexcept
  {
    auto& wav = r.wav;
    const int channels = wav.channels();
    assert(std::ssize(sum) == channels);

    float* val = (float*)alloca(sizeof(float) * channels);
    if(Q_UNLIKELY(!wav.seek_to_pcm_frame(start_frame)))
      return;

    int max = wav.read_pcm_frames_f32(1, val);
    if(Q_UNLIKELY(max == 0))
      return;

    for(int c = 0; c < channels; c++)
    {
      sum[c] = val[c];
    }
  }
};
}
void AudioFile::ViewHandle::frame(
    int64_t start_frame, ossia::small_vector<float, 8>& out) noexcept
{
  SingleFrameComputer _{start_frame, out};
  ossia::visit(_, *this);
}

void AudioFile::ViewHandle::absmax_frame(
    int64_t start_frame, int64_t end_frame, ossia::small_vector<float, 8>& out) noexcept
{
  struct AbsMax
  {
    static constexpr float init(float v) noexcept { return v; }
    constexpr float operator()(float f1, float f2) const noexcept
    {
      return abs_max(f1, f2);
    }
    static constexpr auto init(int64_t v) noexcept { return v; }
    constexpr int64_t operator()(int64_t f1, int64_t f2) const noexcept
    {
      return abs_max(f1, f2);
    }
  };
  FrameComputer<AbsMax, float> _{start_frame, end_frame, out};
  ossia::visit(_, *this);
}

//! Folds the samples of [start_frame, end_frame) into `out`, which already
//! holds the min/max of another range.
void AudioFile::ViewHandle::merge_range(
    int64_t start_frame, int64_t end_frame,
    ossia::small_vector<FloatPair, 8>& out) noexcept
{
  if(start_frame >= end_frame)
    return;

  // Neutral, so that a read which fails leaves `out` as it was rather than
  // dragging it towards zero.
  ossia::small_vector<FloatPair, 8> part(
      out.size(),
      FloatPair{std::numeric_limits<float>::max(), std::numeric_limits<float>::lowest()});

  FrameComputer<MinMax, FloatPair> _{start_frame, end_frame, part};
  ossia::visit(_, *this);

  for(std::size_t c = 0; c < out.size(); c++)
  {
    out[c].first = std::min(out[c].first, part[c].first);
    out[c].second = std::max(out[c].second, part[c].second);
  }
}

//! Whole buckets out of the summary, the partial frames at either end straight
//! from the source.
//!
//! That is exactly the min/max over [start_frame, end_frame) -- the same answer
//! the plain scan gives, bit for bit, because min and max do not care in which
//! order the samples are folded. Being able to assert that is what makes the
//! fast path safe to switch on.
bool AudioFile::ViewHandle::summary_minmax(
    int64_t start_frame, int64_t end_frame,
    ossia::small_vector<FloatPair, 8>& out) noexcept
{
  const WaveformSummary* const s = summary.get();
  if(!s || !s->data)
    return false;
  if(std::ssize(out) != s->channels)
    return false;
  if(start_frame < 0 || end_frame > s->frames)
    return false;

  const int64_t B = s->bucket;

  // Under a few buckets the direct scan is already short, and the partial ends
  // would be most of the work anyway.
  if(end_frame - start_frame < 4 * B)
    return false;

  const int64_t first = (start_frame + B - 1) / B; // first whole bucket
  const int64_t last = end_frame / B;              // one past the last whole one
  if(last <= first || last > s->bucketCount)
    return false;

  for(int c = 0; c < s->channels; c++)
  {
    const FloatPair* const v = s->channel(c);
    float lo = v[first].first, hi = v[first].second;
    for(int64_t b = first + 1; b < last; b++)
    {
      lo = v[b].first < lo ? v[b].first : lo;
      hi = v[b].second > hi ? v[b].second : hi;
    }
    out[c] = {lo, hi};
  }

  merge_range(start_frame, first * B, out);
  merge_range(last * B, end_frame, out);
  return true;
}

bool AudioFile::ViewHandle::supports_summary() const noexcept
{
  struct
  {
    bool operator()(ossia::monostate) const noexcept { return false; }
    bool operator()(const AudioFile::StreamView&) const noexcept { return false; }
    bool operator()(const AudioFile::RAMView& r) const noexcept
    {
      return !r.data.empty();
    }
    bool operator()(const AudioFile::MmapView& r) const noexcept
    {
      return r.wav.channels() > 0;
    }
  } _;
  return ossia::apply(_, static_cast<const view_impl_t&>(*this));
}

bool AudioFile::ViewHandle::build_summary(WaveformSummary& s) noexcept
{
  bool ok = false;
  SummaryBuilder _{s, ok};
  ossia::visit(_, *this);
  return ok;
}

void AudioFile::ViewHandle::minmax_frame(
    int64_t start_frame, int64_t end_frame,
    ossia::small_vector<FloatPair, 8>& out) noexcept
{
  if(summary_minmax(start_frame, end_frame, out))
    return;

  FrameComputer<MinMax, FloatPair> _{start_frame, end_frame, out};
  ossia::visit(_, *this);
}
}
