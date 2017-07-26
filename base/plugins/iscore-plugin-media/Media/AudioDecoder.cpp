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
#include <libavformat/avformat.h>
}

namespace
{
template<AVSampleFormat SampleFormat, int N>
struct ConvertToFloat;

template<>
struct ConvertToFloat<AV_SAMPLE_FMT_S16, 16>
{
        using base_type = int16_t;
        constexpr float operator()(int16_t i) const
        { return (i + .5) / (0x7FFF + .5); }
};


template<>
struct ConvertToFloat<AV_SAMPLE_FMT_S32, 24>
{
        using base_type = const int32_t;
        constexpr float operator()(const int32_t& src_r) const
        {
            return src_r / (float)(std::numeric_limits<int32_t>::max());
            //return impl(&src_r);
        }

      /* may be useful one day
        constexpr static float impl(const int32_t* src)
        {
            return int32_t(src[2] << 24 | src[1] << 16 | src[0] << 8) / (float)(std::numeric_limits<int32_t>::max() - 256);
        }
      */
};

template<>
struct ConvertToFloat<AV_SAMPLE_FMT_S32, 32>
{
        using base_type = int32_t;
        constexpr float operator()(int32_t i) const
        { return i / (float)(std::numeric_limits<int32_t>::max()); }
};

template<>
struct ConvertToFloat<AV_SAMPLE_FMT_FLT, 32>
{
        using base_type = float;
        constexpr float operator()(float i) const
        { return i; }
};


template<int Channels, AVSampleFormat SampleFormat, int SampleSize, bool Planar>
struct Decoder;

template<AVSampleFormat SampleFormat, int SampleSize>
struct Decoder<1, SampleFormat, SampleSize, true>
{
        using converter_t = ConvertToFloat<SampleFormat, SampleSize>;
        void operator()(AudioArray& data, uint8_t** buf, std::size_t n)
        {
            auto dat = reinterpret_cast<typename converter_t::base_type*>(buf[0]);
            data[0].reserve(data[0].size() + n);
            for(int j = 0; j < n; j++)
            {
                data[0].push_back(converter_t{}(dat[j]));
            }
        }
};

template<AVSampleFormat SampleFormat, int SampleSize>
struct Decoder<2, SampleFormat, SampleSize, false>
{
        using converter_t = ConvertToFloat<SampleFormat, SampleSize>;
        void operator()(AudioArray& data, uint8_t** buf, std::size_t n)
        {
            auto dat = reinterpret_cast<typename converter_t::base_type*>(buf[0]);
            data[0].reserve(data[0].size() + n);
            data[1].reserve(data[1].size() + n);
            for(int j = 0; j < 2 * n; )
            {
                data[0].push_back(converter_t{}(dat[j]));
                j++;
                data[1].push_back(converter_t{}(dat[j]));
                j++;
            }
        }
};

template<AVSampleFormat SampleFormat, int SampleSize>
struct Decoder<2, SampleFormat, SampleSize, true>
{
        using converter_t = ConvertToFloat<SampleFormat, SampleSize>;
        void operator()(AudioArray& data, uint8_t** buf, std::size_t n)
        {
            auto dat = reinterpret_cast<typename converter_t::base_type**>(buf);
            data[0].reserve(data[0].size() + n);
            data[1].reserve(data[1].size() + n);
            for(int j = 0; j < n; j++)
            {
                data[0].push_back(converter_t{}(dat[0][j]));
                data[1].push_back(converter_t{}(dat[1][j]));
            }
        }
};

template<>
struct Decoder<2, AV_SAMPLE_FMT_S32, 24, false>
{
        using converter_t = ConvertToFloat<AV_SAMPLE_FMT_S32, 24>;
        void operator()(AudioArray& data, uint8_t** buf, std::size_t n)
        {
          /*
            auto dat = buf.data<const uint8_t>();
            data[0].reserve(data[0].size() + n);
            data[1].reserve(data[1].size() + n);
            for(int j = 0; j < 2 * n; )
            {
                data[0].push_back(converter_t{}(dat[j]));
                j++;
                data[1].push_back(converter_t{}(dat[j]));
                j++;
            }*/
        }
};

struct decode_visitor
{
        AudioArray& data;
        uint8_t** raw{};
        std::size_t length{};
        std::size_t channels{};

        template<typename T>
        void operator()(T decoder)
        {
            decoder(data, raw, length);
        }
};

 static eggs::variant<
 Decoder<1, AV_SAMPLE_FMT_S16P, 16, true>,
 Decoder<2, AV_SAMPLE_FMT_S16P, 16, true>,
 Decoder<2, AV_SAMPLE_FMT_S16, 16, false>,
 Decoder<1, AV_SAMPLE_FMT_S32P, 24, true>,
 Decoder<2, AV_SAMPLE_FMT_S32P, 24, true>,
 Decoder<2, AV_SAMPLE_FMT_S32, 24, false>,
 Decoder<1, AV_SAMPLE_FMT_S32P, 32, true>,
 Decoder<2, AV_SAMPLE_FMT_S32P, 32, true>,
 Decoder<2, AV_SAMPLE_FMT_S32, 32, false>,
 Decoder<1, AV_SAMPLE_FMT_FLTP, 32, true>,
 Decoder<2, AV_SAMPLE_FMT_FLTP, 32, true>,
 Decoder<2, AV_SAMPLE_FMT_FLT, 32, false>
 > make_decoder(AVStream& stream)
{
    int size = stream.codecpar->bits_per_raw_sample;
    int chan = stream.codecpar->channels;
    switch((AVSampleFormat)stream.codecpar->format)
    {
        case AVSampleFormat::AV_SAMPLE_FMT_S16:
        case AVSampleFormat::AV_SAMPLE_FMT_S32:
            switch(size)
            {
                case 16:
                    switch(chan)
                    {
                        case 1: return Decoder<1, AV_SAMPLE_FMT_S16P, 16, true>{};
                        case 2: return Decoder<2, AV_SAMPLE_FMT_S16, 16, false>{};
                        default: return {};
                    }
                case 24:
                    switch(chan)
                    {
                        case 1: return Decoder<1, AV_SAMPLE_FMT_S32P, 24, true>{};
                        case 2: return Decoder<2, AV_SAMPLE_FMT_S32, 24, false>{};
                        default: return {};
                    }
                case 32:
                    switch(chan)
                    {
                        case 1: return Decoder<1, AV_SAMPLE_FMT_S32P, 32, true>{};
                        case 2: return Decoder<2, AV_SAMPLE_FMT_S32, 32, false>{};
                        default: return {};
                    }
                default:
                    return {};
            }

      case AVSampleFormat::AV_SAMPLE_FMT_FLT:
            if(size == 32)
            {
                switch(chan)
                {
                    case 1: return Decoder<1, AV_SAMPLE_FMT_FLTP, 32, true>{};
                    case 2: return Decoder<2, AV_SAMPLE_FMT_FLT, 32, false>{};
                    default: return {};
                }
            }


      case AVSampleFormat::AV_SAMPLE_FMT_S16P:
      case AVSampleFormat::AV_SAMPLE_FMT_S32P:
          switch(size)
          {
              case 16:
                  switch(chan)
                  {
                      case 1: return Decoder<1, AV_SAMPLE_FMT_S16P, 16, true>{};
                      case 2: return Decoder<2, AV_SAMPLE_FMT_S16P, 16, true>{};
                      default: return {};
                  }
              case 24:
                  switch(chan)
                  {
                      case 1: return Decoder<1, AV_SAMPLE_FMT_S32P, 24, true>{};
                      case 2: return Decoder<2, AV_SAMPLE_FMT_S32P, 24, true>{};
                      default: return {};
                  }
              case 32:
                  switch(chan)
                  {
                      case 1: return Decoder<1, AV_SAMPLE_FMT_S32P, 32, true>{};
                      case 2: return Decoder<2, AV_SAMPLE_FMT_S32P, 32, true>{};
                      default: return {};
                  }
              default:
                  return {};
          }

    case AVSampleFormat::AV_SAMPLE_FMT_FLTP:
          if(size == 32)
          {
              switch(chan)
              {
                  case 1: return Decoder<1, AV_SAMPLE_FMT_FLTP, 32, true>{};
                  case 2: return Decoder<2, AV_SAMPLE_FMT_FLTP, 32, true>{};
                  default: return {};
              }
          }



        default:
            return {};
    }
}
}
namespace Media
{

AudioInfo probe_ffmpeg(QString filename)
{
  AudioInfo info;

  av_register_all();
  avcodec_register_all();

  AVFormatContext* fmt_ctx{};

  if(avformat_open_input(&fmt_ctx, filename.toLatin1().constData(), NULL, NULL)!=0)
    return info;

  if(avformat_find_stream_info(fmt_ctx, NULL)<0)
    return info;


  const AVCodec *codec{};
  AVCodecContext *codec_ctx{};
  AVStream* stream{};

  for(std::size_t i = 0; i<fmt_ctx->nb_streams; i++)
  {
    if(fmt_ctx->streams[i]->codecpar->codec_type==AVMEDIA_TYPE_AUDIO) {
      stream = fmt_ctx->streams[i];
      codec_ctx = fmt_ctx->streams[i]->codec;

      qDebug() << "Found audio codec" << avcodec_get_name(fmt_ctx->streams[i]->codec->codec_id);

      codec = avcodec_find_decoder(codec_ctx->codec_id);
      break;
    }
  }
  if(!codec || !codec_ctx)
  {
    qDebug() << codec  << codec_ctx;
    return info;
  }

  info.channels = stream->codecpar->channels;
  info.rate = stream->codecpar->sample_rate;
  info.length = stream->duration;
  info.ok = true;


  avcodec_free_context(&codec_ctx);
  return info;
}


std::pair<std::vector<std::vector<float>>, int> read_ffmpeg(QString filename)
{
  av_register_all();
  avcodec_register_all();

  AVFormatContext* fmt_ctx{};

  if(avformat_open_input(&fmt_ctx, filename.toLatin1().constData(), NULL, NULL)!=0)
    throw std::runtime_error("Couldn't open file");

  if(avformat_find_stream_info(fmt_ctx, NULL)<0)
    throw std::runtime_error("Couldn't find stream information");


  const AVCodec *codec{};
  AVCodecContext *codec_ctx{};
  AVStream* stream{};

  for(int i = 0; i < fmt_ctx->nb_streams; i++)
  {
    if(fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
    {
      stream = fmt_ctx->streams[i];
      codec_ctx = fmt_ctx->streams[i]->codec;

      qDebug() << "Found audio codec" << avcodec_get_name(fmt_ctx->streams[i]->codecpar->codec_id);

      codec_ctx->request_sample_fmt = AV_SAMPLE_FMT_FLTP;
      codec = avcodec_find_decoder(codec_ctx->codec_id);
      break;
    }
  }

  if(!codec || !codec_ctx)
  {
    qDebug() << codec  << codec_ctx;
    return {};
  }

  codec_ctx->request_sample_fmt = AV_SAMPLE_FMT_FLTP;
  if (avcodec_open2(codec_ctx, codec, NULL) < 0)
    throw std::runtime_error("Couldn't open codec");


  AVPacket packet;
  AVFrame *decoded_frame = av_frame_alloc();

  std::vector<std::vector<float>> vec;
  vec.resize(stream->codecpar->channels);
  int rate = stream->codecpar->sample_rate;
  switch(stream->codecpar->format)
  {
    case AVSampleFormat::AV_SAMPLE_FMT_NONE:
      break;

    // non-planar formats
    case AVSampleFormat::AV_SAMPLE_FMT_U8:
      break;
    case AVSampleFormat::AV_SAMPLE_FMT_S16:
      break;
    case AVSampleFormat::AV_SAMPLE_FMT_S32:
      break;
    case AVSampleFormat::AV_SAMPLE_FMT_S64:
      break;
    case AVSampleFormat::AV_SAMPLE_FMT_FLT:
      break;
    case AVSampleFormat::AV_SAMPLE_FMT_DBL:
      break;

    // planar formats
    case AVSampleFormat::AV_SAMPLE_FMT_U8P:
      break;
    case AVSampleFormat::AV_SAMPLE_FMT_S16P:
      break;
    case AVSampleFormat::AV_SAMPLE_FMT_S32P:
      break;
    case AVSampleFormat::AV_SAMPLE_FMT_S64P:
      break;
    case AVSampleFormat::AV_SAMPLE_FMT_FLTP:
      break;
    case AVSampleFormat::AV_SAMPLE_FMT_DBLP:
      break;
  }


  codec_ctx->request_sample_fmt = AV_SAMPLE_FMT_FLTP;
  while(av_read_frame(fmt_ctx, &packet)>=0)
  {
      if(avcodec_send_packet(codec_ctx, &packet) != 0)
        break;
      if(avcodec_receive_frame(codec_ctx, decoded_frame) != 0)
        continue;

      auto buf = reinterpret_cast<int16_t**>(decoded_frame->extended_data);
      for(int i = 0; i < decoded_frame->channels; i++)
      {
        for(int j = 0; j < decoded_frame->nb_samples; j++)
        {
          vec[i].push_back(float(buf[i][j]) / std::numeric_limits<int16_t>::max());
          std::cerr << vec[i].back() << "\n";
        }
      }
  }

  avcodec_free_context(&codec_ctx);
  av_frame_free(&decoded_frame);
  return {vec, rate};
}

AudioDecoder::AudioDecoder()
{

}

AudioInfo AudioDecoder::probe(const QString& path)
{
  AudioInfo info = probe_ffmpeg(path);
  return info;
}

void AudioDecoder::decode(const QString& path)
{
  auto res = read_ffmpeg(path);
  data = res.first;
  sampleRate = res.second;
  ready = true;
  return;
}

}
