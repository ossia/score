#include "AudioDecoder.hpp"
#include <QApplication>
#include <QTimer>
#include <eggs/variant.hpp>





#include <QFile>
#include <QFileInfo>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/mem.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavformat/avformat.h>
}

namespace
{
static const constexpr std::size_t dynamic_channels = std::numeric_limits<std::size_t>::max();

template<typename SampleFormat, int N>
constexpr float convert_sample(SampleFormat i);

template<>
constexpr float convert_sample<int16_t, 16>(int16_t i)
{
  return (i + .5) / (0x7FFF + .5);
}

template<>
constexpr float convert_sample<int32_t, 24>(int32_t i)
{
  return ((int32_t)i >> 8) / ((float)std::numeric_limits<int32_t>::max() / 256.);
}

template<>
constexpr float convert_sample<int32_t, 32>(int32_t i)
{
  return i / (float)(std::numeric_limits<int32_t>::max());
}

template<>
constexpr float convert_sample<float, 32>(float i)
{
  return i;
}

template<typename SampleFormat, std::size_t Channels, std::size_t SampleSize, bool Planar>
struct Decoder;

template<typename SampleFormat, std::size_t SampleSize>
struct Decoder<SampleFormat, 1, SampleSize, true>
{
    void operator()(AudioArray& data, uint8_t** buf, std::size_t n)
    {
      auto dat = reinterpret_cast<SampleFormat*>(buf[0]);
      for(std::size_t j = 0; j < n; j++)
      {
        data[0].push_back(convert_sample<SampleFormat, SampleSize>(dat[j]));
      }
    }
};

template<typename SampleFormat, std::size_t Channels, std::size_t SampleSize>
struct Decoder<SampleFormat, Channels, SampleSize, false>
{
    void operator()(AudioArray& data, uint8_t** buf, std::size_t n)
    {
      auto dat = reinterpret_cast<SampleFormat*>(buf[0]);
      for(std::size_t chan = 0; chan < Channels; chan++)
      {
        data[chan].reserve(data[chan].size() + n);
      }

      for(std::size_t j = 0; j < Channels * n; )
      {
        for(std::size_t chan = 0; chan < Channels; chan++)
        {
          data[chan].push_back(convert_sample<SampleFormat, SampleSize>(dat[j]));
          j++;
        }
      }
    }
};

template<typename SampleFormat, std::size_t Channels, std::size_t SampleSize>
struct Decoder<SampleFormat, Channels, SampleSize, true>
{
    void operator()(AudioArray& data, uint8_t** buf, std::size_t n)
    {
      auto dat = reinterpret_cast<SampleFormat**>(buf);
      for(std::size_t chan = 0; chan < Channels; chan++)
      {
        data[chan].reserve(data[chan].size() + n);
        for(std::size_t j = 0; j < n; j++)
        {
          data[chan].push_back(convert_sample<SampleFormat, SampleSize>(dat[chan][j]));
        }
      }
    }
};

template<typename SampleFormat, std::size_t SampleSize>
struct Decoder<SampleFormat, dynamic_channels, SampleSize, false>
{
    std::size_t Channels{};
    void operator()(AudioArray& data, uint8_t** buf, std::size_t n)
    {
      auto dat = reinterpret_cast<SampleFormat*>(buf[0]);
      for(std::size_t chan = 0; chan < Channels; chan++)
      {
        data[chan].reserve(data[chan].size() + n);
      }

      for(std::size_t j = 0; j < Channels * n; )
      {
        for(std::size_t chan = 0; chan < Channels; chan++)
        {
          data[chan].push_back(convert_sample<SampleFormat, SampleSize>(dat[j]));
          j++;
        }
      }
    }
};

template<typename SampleFormat, std::size_t SampleSize>
struct Decoder<SampleFormat, dynamic_channels, SampleSize, true>
{
    std::size_t Channels{};
    void operator()(AudioArray& data, uint8_t** buf, std::size_t n)
    {
      auto dat = reinterpret_cast<SampleFormat**>(buf);
      for(std::size_t chan = 0; chan < Channels; chan++)
      {
        data[chan].reserve(data[chan].size() + n);
        for(std::size_t j = 0; j < n; j++)
        {
          data[chan].push_back(convert_sample<SampleFormat, SampleSize>(dat[chan][j]));
        }
      }
    }
};


using decoder_t = eggs::variant<
Decoder<int16_t, 1, 16, true>,
Decoder<int16_t, 2, 16, true>,
Decoder<int16_t, 2, 16, false>,
Decoder<int16_t, dynamic_channels, 16, true>,
Decoder<int16_t, dynamic_channels, 16, false>,

Decoder<int32_t, 1, 24, true>,
Decoder<int32_t, 2, 24, true>,
Decoder<int32_t, 2, 24, false>,
Decoder<int32_t, dynamic_channels, 24, true>,
Decoder<int32_t, dynamic_channels, 24, false>,

Decoder<int32_t, 1, 32, true>,
Decoder<int32_t, 2, 32, true>,
Decoder<int32_t, 2, 32, false>,
Decoder<int32_t, dynamic_channels, 32, true>,
Decoder<int32_t, dynamic_channels, 32, false>,

Decoder<float,   1, 32, true>,
Decoder<float,   2, 32, true>,
Decoder<float,   2, 32, false>,
Decoder<float,   dynamic_channels, 32, true>,
Decoder<float,   dynamic_channels, 32, false>
>;


template<std::size_t N>
decoder_t make_N_decoder(AVStream& stream)
{
  const int size = stream.codecpar->bits_per_raw_sample;

  if(size == 0 || size == 16 || size == 32)
  {
    switch((AVSampleFormat)stream.codecpar->format)
    {
      case AVSampleFormat::AV_SAMPLE_FMT_S16:
        return Decoder<int16_t, N, 16, false>{};
      case AVSampleFormat::AV_SAMPLE_FMT_S32:
        return Decoder<int32_t, N, 32, false>{};
      case AVSampleFormat::AV_SAMPLE_FMT_FLT:
        return Decoder<float, N, 32, false>{};

      case AVSampleFormat::AV_SAMPLE_FMT_S16P:
        return Decoder<int16_t, N, 16, true>{};
      case AVSampleFormat::AV_SAMPLE_FMT_S32P:
        return Decoder<int32_t, N, 32, true>{};
      case AVSampleFormat::AV_SAMPLE_FMT_FLTP:
        return Decoder<float, N, 32, true>{};
      default:
        return {};
    }
  }
  else if(size == 24)
  {
    switch((AVSampleFormat)stream.codecpar->format)
    {
      case AVSampleFormat::AV_SAMPLE_FMT_S32:
        return Decoder<int32_t, N, 24, false>{};
      case AVSampleFormat::AV_SAMPLE_FMT_S32P:
        return Decoder<int32_t, N, 24, true>{};
      default:
        break;
    }
  }

  return {};
}

decoder_t make_dynamic_decoder(AVStream& stream)
{
  const int size = stream.codecpar->bits_per_raw_sample;
  const std::size_t channels = stream.codecpar->channels;

  if(size == 0 || size == 16 || size == 32)
  {
    switch((AVSampleFormat)stream.codecpar->format)
    {
      case AVSampleFormat::AV_SAMPLE_FMT_S16:
        return Decoder<int16_t, dynamic_channels, 16, false>{channels};
      case AVSampleFormat::AV_SAMPLE_FMT_S32:
        return Decoder<int32_t, dynamic_channels, 32, false>{channels};
      case AVSampleFormat::AV_SAMPLE_FMT_FLT:
        return Decoder<float, dynamic_channels, 32, false>{channels};

      case AVSampleFormat::AV_SAMPLE_FMT_S16P:
        return Decoder<int16_t, dynamic_channels, 16, true>{channels};
      case AVSampleFormat::AV_SAMPLE_FMT_S32P:
        return Decoder<int32_t, dynamic_channels, 32, true>{channels};
      case AVSampleFormat::AV_SAMPLE_FMT_FLTP:
        return Decoder<float, dynamic_channels, 32, true>{channels};
      default:
        return {};
    }
  }
  else if(size == 24)
  {
    switch((AVSampleFormat)stream.codecpar->format)
    {
      case AVSampleFormat::AV_SAMPLE_FMT_S32:
        return Decoder<int32_t, dynamic_channels, 24, false>{channels};
      case AVSampleFormat::AV_SAMPLE_FMT_S32P:
        return Decoder<int32_t, dynamic_channels, 24, true>{channels};
      default:
        break;
    }
  }

  return {};
}

// Simple mono case: everything is planar
template<>
decoder_t make_N_decoder<1>(AVStream& stream)
{
  const int size = stream.codecpar->bits_per_raw_sample;

  if(size == 0 || size == 16 || size == 32)
  {
    switch((AVSampleFormat)stream.codecpar->format)
    {
      case AVSampleFormat::AV_SAMPLE_FMT_S16:
      case AVSampleFormat::AV_SAMPLE_FMT_S16P:
        return Decoder<int16_t, 1, 16, true>{};
      case AVSampleFormat::AV_SAMPLE_FMT_S32:
      case AVSampleFormat::AV_SAMPLE_FMT_S32P:
        return Decoder<int32_t, 1, 32, true>{};
      case AVSampleFormat::AV_SAMPLE_FMT_FLT:
      case AVSampleFormat::AV_SAMPLE_FMT_FLTP:
        return Decoder<float, 1, 32, true>{};
      default:
        return {};
    }
  }
  else if(size == 24)
  {
    return Decoder<int32_t, 1, 24, true>{};
  }
  else
  {
    return {};
  }
}

 static decoder_t make_decoder(AVStream& stream)
{
    switch(stream.codecpar->channels)
    {
      case 1: return make_N_decoder<1>(stream);
      case 2: return make_N_decoder<2>(stream);
      case 0: return {};
      default: return make_dynamic_decoder(stream);
    }
}
}

namespace Media
{
AudioDecoder::AudioDecoder()
{

}
struct AVCodecContext_Free {
    void operator()(AVCodecContext* ctx)
    { avcodec_free_context(&ctx); }
};
struct AVFormatContext_Free {
    void operator()(AVFormatContext* ctx)
    { avformat_free_context(ctx); }
};
struct AVFrame_Free {
    void operator()(AVFrame* frame)
    { av_frame_free(&frame); }
};
using AVFormatContext_ptr = std::unique_ptr<AVFormatContext, AVFormatContext_Free>;
using AVCodecContext_ptr = std::unique_ptr<AVCodecContext, AVCodecContext_Free>;
using AVFrame_ptr = std::unique_ptr<AVFrame, AVFrame_Free>;


AVFormatContext_ptr open_audio(const QString& path)
{
  AVFormatContext* fmt_ctx_ptr{};
  auto ret = avformat_open_input(&fmt_ctx_ptr, path.toLatin1().constData(), nullptr, nullptr);
  if(ret != 0)
    throw std::runtime_error("Couldn't open file");

  return AVFormatContext_ptr{fmt_ctx_ptr};
}

ossia::optional<AudioInfo> AudioDecoder::probe(const QString& path)
{
  av_register_all();
  avcodec_register_all();

  auto fmt_ctx = open_audio(path);

  if(avformat_find_stream_info(fmt_ctx.get(), nullptr) < 0)
    return {};

  for(std::size_t i = 0; i < fmt_ctx->nb_streams; i++)
  {
    if(fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
    {
      auto stream = fmt_ctx->streams[i];
      AudioInfo info;
      info.channels = stream->codecpar->channels;
      if(info.channels == 0)
        return {};
      info.rate = stream->codecpar->sample_rate;
      info.length = read_length(path);

      if(info.rate != 44100)
        info.length = av_rescale_rnd(info.length, 44100, info.rate, AV_ROUND_UP);
      return info;
    }
  }

  return {};
}
std::size_t AudioDecoder::read_length(const QString& path)
{
  av_register_all();
  avcodec_register_all();

  auto fmt_ctx = open_audio(path);

  auto ret = avformat_find_stream_info(fmt_ctx.get(), NULL);
  if(ret != 0)
    throw std::runtime_error("Couldn't find stream information");

  for(std::size_t i = 0; i < fmt_ctx->nb_streams; i++)
  {
    if(fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
    {
      auto stream = fmt_ctx->streams[i];
      auto codec = avcodec_find_decoder(fmt_ctx->streams[i]->codecpar->codec_id);
      if(!codec)
        throw std::runtime_error("Couldn't find codec");

      AVCodecContext_ptr codec_ctx{avcodec_alloc_context3(codec)};
      if(!codec_ctx)
        throw std::runtime_error("Couldn't allocate codec context");

      ret = avcodec_parameters_to_context(codec_ctx.get(), fmt_ctx->streams[i]->codecpar);
      if(ret != 0)
        throw std::runtime_error("Couldn't copy codec data");

      ret = avcodec_open2(codec_ctx.get(), codec, nullptr);
      if (ret != 0)
        throw std::runtime_error("Couldn't open codec");

      data.resize(stream->codecpar->channels);
      sampleRate = stream->codecpar->sample_rate;
      AVPacket packet;
      AVFrame_ptr frame{av_frame_alloc()};

      int64_t pos = 0;
      while(av_read_frame(fmt_ctx.get(), &packet)>=0)
      {
        ret = avcodec_send_packet(codec_ctx.get(), &packet);
        if(ret != 0)
          break;
        ret = avcodec_receive_frame(codec_ctx.get(), frame.get());

        if(ret == 0)
        {
          pos += frame->nb_samples;
        }
        else if(ret == AVERROR(EAGAIN))
        {
          continue;
        }
        else if(ret == AVERROR_EOF)
        {
          pos += frame->nb_samples;
          break;
        }
        else
        {
          break;
        }
      }

      // Flush
      avcodec_send_packet(codec_ctx.get(), NULL);
      return pos;

    }
  }
  return 0;
}

void AudioDecoder::decode(const QString& path)
{
  av_register_all();
  avcodec_register_all();

  auto fmt_ctx = open_audio(path);

  auto ret = avformat_find_stream_info(fmt_ctx.get(), NULL);
  if(ret != 0)
    throw std::runtime_error("Couldn't find stream information");

  for(std::size_t i = 0; i < fmt_ctx->nb_streams; i++)
  {
    if(fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
    {
      auto stream = fmt_ctx->streams[i];
      auto codec = avcodec_find_decoder(fmt_ctx->streams[i]->codecpar->codec_id);
      if(!codec)
        throw std::runtime_error("Couldn't find codec");

      AVCodecContext_ptr codec_ctx{avcodec_alloc_context3(codec)};
      if(!codec_ctx)
        throw std::runtime_error("Couldn't allocate codec context");

      ret = avcodec_parameters_to_context(codec_ctx.get(), fmt_ctx->streams[i]->codecpar);
      if(ret != 0)
        throw std::runtime_error("Couldn't copy codec data");

      ret = avcodec_open2(codec_ctx.get(), codec, nullptr);
      if (ret != 0)
        throw std::runtime_error("Couldn't open codec");

      data.resize(stream->codecpar->channels);
      sampleRate = stream->codecpar->sample_rate;

      if(auto decoder = make_decoder(*stream))
      {
        eggs::variants::apply([&] (auto& dec)
        {
          AVPacket packet;
          AVFrame_ptr frame{av_frame_alloc()};

          while(av_read_frame(fmt_ctx.get(), &packet) >= 0)
          {
              ret = avcodec_send_packet(codec_ctx.get(), &packet);
              if(ret != 0)
                break;
              ret = avcodec_receive_frame(codec_ctx.get(), frame.get());

              if(ret == 0)
              {
                dec(data, frame->extended_data, frame->nb_samples);
              }
              else if(ret == AVERROR(EAGAIN))
              {
                continue;
              }
              else if(ret == AVERROR_EOF)
              {
                dec(data, frame->extended_data, frame->nb_samples);
                break;
              }
              else
              {
                break;
              }
          }

          // Flush
          avcodec_send_packet(codec_ctx.get(), NULL);
        }, decoder);
      }

      if(sampleRate != 44100)
      {
        AudioArray out;
        out.resize(data.size());

        auto new_len = av_rescale_rnd(data[0].size(), 44100, sampleRate, AV_ROUND_UP);
        for(std::size_t i = 0; i < data.size(); i++)
        {
          auto& chan = out[i];
          chan.resize(new_len);

          SwrContext *swr = swr_alloc_set_opts(
                              NULL,
                              AV_CH_LAYOUT_MONO,
                              AV_SAMPLE_FMT_FLTP,
                              44100,
                              AV_CH_LAYOUT_MONO,
                              AV_SAMPLE_FMT_FLTP,
                              sampleRate,
                              0,
                              NULL);

          swr_init(swr);
          float* out_ptr = out[i].data();
          float* in_ptr = data[i].data();
          swr_convert(swr, (uint8_t**)&out_ptr, chan.size(), (const uint8_t**)&in_ptr, data[i].size());
          swr_free(&swr);
        }

        data = std::move(out);
      }

      break;
    }
  }

  ready = true;
  return;
}

}
