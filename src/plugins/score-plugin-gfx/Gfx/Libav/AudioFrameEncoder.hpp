#pragma once

#include <Media/Libav.hpp>
#if SCORE_HAS_LIBAV
extern "C" {
#include <libavutil/frame.h>
}

#include <ossia/dataflow/float_to_sample.hpp>

namespace Gfx
{

struct AudioFrameEncoder
{
  explicit AudioFrameEncoder(int target_buffer_size)
      : target_buffer_size{target_buffer_size}
  {
  }

  // We assume that vec has correct channel count here
  // Also that vec.size() > 0
  virtual void add_frame(AVFrame& frame, const std::span<ossia::float_vector> vec) = 0;
  virtual ~AudioFrameEncoder() = default;
  int target_buffer_size{};
};

struct S16IAudioFrameEncoder final : AudioFrameEncoder
{
  using AudioFrameEncoder::AudioFrameEncoder;

  void add_frame(AVFrame& frame, const std::span<ossia::float_vector> vec) override
  {
    const int channels = vec.size();
    const int frames = vec[0].size();
    auto* ptr = reinterpret_cast<int16_t*>(frame.data[0]);
    for(int i = 0; i < frames; i++)
      for(int c = 0; c < channels; c++)
        *ptr++ = ossia::float_to_sample<int16_t, 16>(vec[c][i]);
  }
};

struct S24IAudioFrameEncoder final : AudioFrameEncoder
{
  using AudioFrameEncoder::AudioFrameEncoder;

  void add_frame(AVFrame& frame, const std::span<ossia::float_vector> vec) override
  {
    const int channels = vec.size();
    const int frames = vec[0].size();
    auto* ptr = reinterpret_cast<int32_t*>(frame.data[0]);
    for(int i = 0; i < frames; i++)
      for(int c = 0; c < channels; c++)
        *ptr++ = ossia::float_to_sample<int32_t, 24>(vec[c][i]);
  }
};

struct S32IAudioFrameEncoder final : AudioFrameEncoder
{
  using AudioFrameEncoder::AudioFrameEncoder;

  void add_frame(AVFrame& frame, const std::span<ossia::float_vector> vec) override
  {
    const int channels = vec.size();
    const int frames = vec[0].size();
    auto* ptr = reinterpret_cast<int32_t*>(frame.data[0]);
    for(int i = 0; i < frames; i++)
      for(int c = 0; c < channels; c++)
        *ptr++ = ossia::float_to_sample<int32_t, 32>(vec[c][i]);
  }
};

struct FltIAudioFrameEncoder final : AudioFrameEncoder
{
  using AudioFrameEncoder::AudioFrameEncoder;

  void add_frame(AVFrame& frame, const std::span<ossia::float_vector> vec) override
  {
    const int channels = vec.size();
    const int frames = vec[0].size();
    auto* ptr = reinterpret_cast<float*>(frame.data[0]);
    for(int i = 0; i < frames; i++)
      for(int c = 0; c < channels; c++)
        *ptr++ = vec[c][i];
  }
};

struct DblIAudioFrameEncoder final : AudioFrameEncoder
{
  using AudioFrameEncoder::AudioFrameEncoder;

  void add_frame(AVFrame& frame, const std::span<ossia::float_vector> vec) override
  {
    const int channels = vec.size();
    const int frames = vec[0].size();
    auto* ptr = reinterpret_cast<double*>(frame.data[0]);
    for(int i = 0; i < frames; i++)
      for(int c = 0; c < channels; c++)
        *ptr++ = vec[c][i];
  }
};

struct FltPAudioFrameEncoder final : AudioFrameEncoder
{
  using AudioFrameEncoder::AudioFrameEncoder;

  void add_frame(AVFrame& frame, const std::span<ossia::float_vector> vec) override
  {
    const int channels = vec.size();
    // Copy into the AVFrame's own planar buffers (allocated by av_frame_get_buffer)
    for(int i = 0; i < channels; ++i)
    {
      uint8_t* dst = (i < AV_NUM_DATA_POINTERS) ? frame.data[i]
                                                 : frame.extended_data[i];
      if(dst)
      {
        const int bytes = vec[i].size() * sizeof(float);
        std::memcpy(dst, vec[i].data(), bytes);
      }
    }
  }
};

struct S16PAudioFrameEncoder final : AudioFrameEncoder
{
  using AudioFrameEncoder::AudioFrameEncoder;

  void add_frame(AVFrame& frame, const std::span<ossia::float_vector> vec) override
  {
    const int channels = vec.size();
    const int frames = vec[0].size();
    for(int i = 0; i < channels; ++i)
    {
      auto* dst = reinterpret_cast<int16_t*>(
          (i < AV_NUM_DATA_POINTERS) ? frame.data[i] : frame.extended_data[i]);
      if(dst)
        for(int j = 0; j < frames; ++j)
          dst[j] = ossia::float_to_sample<int16_t, 16>(vec[i][j]);
    }
  }
};

struct S32PAudioFrameEncoder final : AudioFrameEncoder
{
  using AudioFrameEncoder::AudioFrameEncoder;

  void add_frame(AVFrame& frame, const std::span<ossia::float_vector> vec) override
  {
    const int channels = vec.size();
    const int frames = vec[0].size();
    for(int i = 0; i < channels; ++i)
    {
      auto* dst = reinterpret_cast<int32_t*>(
          (i < AV_NUM_DATA_POINTERS) ? frame.data[i] : frame.extended_data[i]);
      if(dst)
        for(int j = 0; j < frames; ++j)
          dst[j] = ossia::float_to_sample<int32_t, 32>(vec[i][j]);
    }
  }
};

struct DblPAudioFrameEncoder final : AudioFrameEncoder
{
  using AudioFrameEncoder::AudioFrameEncoder;

  void add_frame(AVFrame& frame, const std::span<ossia::float_vector> vec) override
  {
    const int channels = vec.size();
    const int frames = vec[0].size();
    for(int i = 0; i < channels; ++i)
    {
      auto* dst = reinterpret_cast<double*>(
          (i < AV_NUM_DATA_POINTERS) ? frame.data[i] : frame.extended_data[i]);
      if(dst)
        for(int j = 0; j < frames; ++j)
          dst[j] = static_cast<double>(vec[i][j]);
    }
  }
};
}
#endif
