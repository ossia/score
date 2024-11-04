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

  virtual ~AudioFrameEncoder() = default;

  // We assume that vec has correct channel count here
  // Also that vec.size() > 0
  virtual void add_frame(AVFrame& frame, const tcb::span<ossia::float_vector> vec) = 0;
  virtual ~AudioFrameEncoder() = default;
  int target_buffer_size{};
};

struct S16IAudioFrameEncoder final : AudioFrameEncoder
{
  using AudioFrameEncoder::AudioFrameEncoder;

  boost::container::vector<int16_t> data;

  void add_frame(AVFrame& frame, const tcb::span<ossia::float_vector> vec) override
  {
    const int channels = vec.size();
    const int frames = vec[0].size();
    data.clear();
    data.resize(frames * channels, boost::container::default_init);
    auto ptr = data.data();
    for(int i = 0; i < frames; i++)
      for(int c = 0; c < channels; c++)
        *ptr++ = ossia::float_to_sample<int16_t, 16>(vec[c][i]);

    frame.data[0] = (uint8_t*)data.data();
    frame.data[1] = nullptr;
  }
};

struct S24IAudioFrameEncoder final : AudioFrameEncoder
{
  using AudioFrameEncoder::AudioFrameEncoder;

  boost::container::vector<int32_t> data;
  void add_frame(AVFrame& frame, const tcb::span<ossia::float_vector> vec) override
  {
    const int channels = vec.size();
    const int frames = vec[0].size();
    data.clear();
    data.resize(frames * channels, boost::container::default_init);
    auto ptr = data.data();
    for(int i = 0; i < frames; i++)
      for(int c = 0; c < channels; c++)
        *ptr++ = ossia::float_to_sample<int32_t, 24>(vec[c][i]);

    frame.data[0] = (uint8_t*)data.data();
    frame.data[1] = nullptr;
  }
};

struct S32IAudioFrameEncoder final : AudioFrameEncoder
{
  using AudioFrameEncoder::AudioFrameEncoder;

  boost::container::vector<int32_t> data;
  void add_frame(AVFrame& frame, const tcb::span<ossia::float_vector> vec) override
  {
    const int channels = vec.size();
    const int frames = vec[0].size();
    data.clear();
    data.resize(frames * channels, boost::container::default_init);
    auto ptr = data.data();
    for(int i = 0; i < frames; i++)
      for(int c = 0; c < channels; c++)
        *ptr++ = ossia::float_to_sample<int32_t, 32>(vec[c][i]);

    frame.data[0] = (uint8_t*)data.data();
    frame.data[1] = nullptr;
  }
};

struct FltIAudioFrameEncoder final : AudioFrameEncoder
{
  using AudioFrameEncoder::AudioFrameEncoder;

  boost::container::vector<float> data;

  void add_frame(AVFrame& frame, const tcb::span<ossia::float_vector> vec) override
  {
    const int channels = vec.size();
    const int frames = vec[0].size();
    data.clear();
    data.resize(frames * channels, boost::container::default_init);
    auto ptr = data.data();
    for(int i = 0; i < frames; i++)
      for(int c = 0; c < channels; c++)
        *ptr++ = vec[c][i];

    frame.data[0] = (uint8_t*)data.data();
    frame.data[1] = nullptr;
  }
};

struct DblIAudioFrameEncoder final : AudioFrameEncoder
{
  using AudioFrameEncoder::AudioFrameEncoder;

  boost::container::vector<double> data;

  void add_frame(AVFrame& frame, const tcb::span<ossia::float_vector> vec) override
  {
    const int channels = vec.size();
    const int frames = vec[0].size();
    data.clear();
    data.resize(frames * channels, boost::container::default_init);
    auto ptr = data.data();
    for(int i = 0; i < frames; i++)
      for(int c = 0; c < channels; c++)
        *ptr++ = vec[c][i];

    frame.data[0] = (uint8_t*)data.data();
    frame.data[1] = nullptr;
  }
};

struct FltPAudioFrameEncoder final : AudioFrameEncoder
{
  using AudioFrameEncoder::AudioFrameEncoder;

  void add_frame(AVFrame& frame, const tcb::span<ossia::float_vector> vec) override
  {
    const int channels = vec.size();
    if(channels <= AV_NUM_DATA_POINTERS)
    {
      for(int i = 0; i < channels; ++i)
      {
        frame.data[i] = reinterpret_cast<uint8_t*>(vec[i].data());
      }
    }
    else
    {
      // FIXME where does this get freed???
      frame.extended_data
          = static_cast<uint8_t**>(av_malloc(channels * sizeof(*frame.extended_data)));
      int i = 0;
      for(; i < AV_NUM_DATA_POINTERS; ++i)
      {
        frame.data[i] = reinterpret_cast<uint8_t*>(vec[i].data());
        frame.extended_data[i] = reinterpret_cast<uint8_t*>(vec[i].data());
      }
      for(; i < channels; ++i)
        frame.extended_data[i] = reinterpret_cast<uint8_t*>(vec[i].data());
    }
  }
};
}
#endif
