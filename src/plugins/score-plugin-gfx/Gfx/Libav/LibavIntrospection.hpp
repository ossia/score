#pragma once

#include <Media/Libav.hpp>
#if SCORE_HAS_LIBAV
#include <Gfx/Libav/LibavOutputSettings.hpp>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
}

namespace Gfx
{
struct ACodecInfo
{
  explicit ACodecInfo(const AVCodec* codec) noexcept
      : codec{codec}
  {
  }
  const AVCodec* codec{};
};
struct VCodecInfo
{
  explicit VCodecInfo(const AVCodec* codec) noexcept
      : codec{codec}
  {
  }
  const AVCodec* codec{};
};
struct MuxerInfo
{
  explicit MuxerInfo(const AVOutputFormat* format) noexcept
      : format{format}
  {
  }
  const AVOutputFormat* format{};
  ossia::small_vector<const ACodecInfo*, 4> acodecs;
  ossia::small_vector<const VCodecInfo*, 4> vcodecs;
};

struct LibavIntrospection
{
  std::vector<VCodecInfo> vcodecs;
  std::vector<ACodecInfo> acodecs;

  std::vector<MuxerInfo> muxers;

  LibavIntrospection()
  {
    vcodecs.reserve(500);
    acodecs.reserve(350);
    muxers.reserve(200);

    void* opaque = nullptr;
    while(auto codec = av_codec_iterate(&opaque))
    {
      if(!av_codec_is_encoder(codec))
        continue;

      if(codec->type == AVMediaType::AVMEDIA_TYPE_AUDIO)
        acodecs.emplace_back(codec);
      if(codec->type == AVMediaType::AVMEDIA_TYPE_VIDEO)
        vcodecs.emplace_back(codec);
    }

    opaque = nullptr;
    while(auto format = av_muxer_iterate(&opaque))
    {
      MuxerInfo fmt{format};
      for(const auto& codec : vcodecs)
      {
        if(avformat_query_codec(format, codec.codec->id, FF_COMPLIANCE_STRICT) == 1)
        {
          fmt.vcodecs.push_back(&codec);
        }
      }
      for(const auto& codec : acodecs)
      {
        if(avformat_query_codec(format, codec.codec->id, FF_COMPLIANCE_STRICT) == 1)
        {
          fmt.acodecs.push_back(&codec);
        }
      }
      if(fmt.vcodecs.empty() && fmt.acodecs.empty())
        continue;
      muxers.push_back(std::move(fmt));
    }
  }

  const MuxerInfo*
  findMuxer(const QString& name, const QString& long_name) const noexcept
  {
    for(auto& obj : muxers)
    {
      if(obj.format->long_name)
      {
        if(obj.format->name == name && obj.format->long_name == long_name)
          return &obj;
      }
    }
    for(auto& obj : muxers)
    {
      if(obj.format->name == name)
        return &obj;
    }
    return nullptr;
  }

  const VCodecInfo*
  findVideoCodec(const QString& name, const QString& long_name) const noexcept
  {
    for(auto& obj : vcodecs)
    {
      if(obj.codec->long_name)
      {
        if(obj.codec->name == name && obj.codec->long_name == long_name)
          return &obj;
      }
    }
    for(auto& obj : vcodecs)
    {
      if(obj.codec->name == name)
        return &obj;
    }
    return nullptr;
  }

  const ACodecInfo*
  findAudioCodec(const QString& name, const QString& long_name) const noexcept
  {
    for(auto& obj : acodecs)
    {
      if(obj.codec->long_name)
      {
        if(obj.codec->name == name && obj.codec->long_name == long_name)
          return &obj;
      }
    }
    for(auto& obj : acodecs)
    {
      if(obj.codec->name == name)
        return &obj;
    }
    return nullptr;
  }

  static const LibavIntrospection& instance() noexcept
  {
    static const LibavIntrospection p;
    return p;
  }
};

}
#endif
