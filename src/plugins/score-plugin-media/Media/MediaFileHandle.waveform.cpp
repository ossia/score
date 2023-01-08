#include <Media/MediaFileHandle.hpp>

#include <ossia/detail/libav.hpp>
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
      int num_elems = buffer_size * channels;
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

void AudioFile::ViewHandle::minmax_frame(
    int64_t start_frame, int64_t end_frame,
    ossia::small_vector<FloatPair, 8>& out) noexcept
{
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
      return std::make_pair(
          f1.first < f2 ? f1.first : f2, f1.second > f2 ? f1.second : f2);
    }
  };

  FrameComputer<MinMax, FloatPair> _{start_frame, end_frame, out};
  ossia::visit(_, *this);
}
}
