#include "AudioDecoder.hpp"

#include <Media/Sound/SoundModel.hpp>
#include <Media/Libav.hpp>

#include <score/tools/Debug.hpp>

#include <QDebug>
#include <QHash>

#include <cmath>
#include <eggs/variant.hpp>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if SCORE_HAS_LIBAV
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/frame.h>
#include <libavutil/mem.h>
#include <libswresample/swresample.h>
}
#endif

#include <wobjectimpl.h>
W_OBJECT_IMPL(Media::AudioDecoder)
#if SCORE_HAS_LIBAV
namespace
{
using namespace Media;
static const constexpr std::size_t dynamic_channels = std::numeric_limits<std::size_t>::max();

template <typename SampleFormat, int N>
constexpr audio_sample convert_sample(SampleFormat i);

template <>
constexpr audio_sample convert_sample<int16_t, 16>(int16_t i)
{
  if constexpr (std::is_same_v<audio_sample, float>)
    return (i + .5f) / (0x7FFF + .5f);
  else
    return (i + .5) / (0x7FFF + .5);
}

template <>
constexpr audio_sample convert_sample<int32_t, 24>(int32_t i)
{
  if constexpr (std::is_same_v<audio_sample, float>)
    return ((int32_t)i >> 8) / ((audio_sample)std::numeric_limits<int32_t>::max() / 256.f);
  else
    return ((int32_t)i >> 8) / ((audio_sample)std::numeric_limits<int32_t>::max() / 256.);
}

template <>
constexpr audio_sample convert_sample<int32_t, 32>(int32_t i)
{
  return i / (audio_sample)(std::numeric_limits<int32_t>::max());
}

template <>
constexpr audio_sample convert_sample<float, 32>(float i)
{
  return i;
}

template <>
constexpr audio_sample convert_sample<double, 64>(double i)
{
  return i;
}

template <typename SampleFormat, std::size_t Channels, std::size_t SampleSize, bool Planar>
struct Decoder;

template <typename SampleFormat, std::size_t SampleSize>
struct Decoder<SampleFormat, 1, SampleSize, true>
{
  void operator()(audio_array& data, std::size_t curpos, uint8_t** buf, std::size_t n)
  {
    auto dat = reinterpret_cast<SampleFormat*>(buf[0]);
    for (std::size_t j = 0; j < n; j++)
    {
      data[0][curpos + j] = convert_sample<SampleFormat, SampleSize>(dat[j]);
    }
  }
};

template <typename SampleFormat, std::size_t Channels, std::size_t SampleSize>
struct Decoder<SampleFormat, Channels, SampleSize, false>
{
  void operator()(audio_array& data, std::size_t curpos, uint8_t** buf, std::size_t n)
  {
    auto dat = reinterpret_cast<SampleFormat*>(buf[0]);

    int i = 0;
    for (std::size_t j = 0; j < Channels * n;)
    {
      std::size_t cur_j = curpos + i;
      for (std::size_t chan = 0; chan < Channels; chan++)
      {
        data[chan][cur_j] = convert_sample<SampleFormat, SampleSize>(dat[j]);
        j++;
      }
      i++;
    }
  }
};

template <typename SampleFormat, std::size_t Channels, std::size_t SampleSize>
struct Decoder<SampleFormat, Channels, SampleSize, true>
{
  void operator()(audio_array& data, std::size_t curpos, uint8_t** buf, std::size_t n)
  {
    auto dat = reinterpret_cast<SampleFormat**>(buf);
    for (std::size_t chan = 0; chan < Channels; chan++)
    {
      for (std::size_t j = 0; j < n; j++)
      {
        data[chan][curpos + j] = convert_sample<SampleFormat, SampleSize>(dat[chan][j]);
      }
    }
  }
};

template <typename SampleFormat, std::size_t SampleSize>
struct Decoder<SampleFormat, dynamic_channels, SampleSize, false>
{
  std::size_t Channels{};
  void operator()(audio_array& data, std::size_t curpos, uint8_t** buf, std::size_t n)
  {
    auto dat = reinterpret_cast<SampleFormat*>(buf[0]);

    int i = 0;
    for (std::size_t j = 0; j < Channels * n;)
    {
      std::size_t cur_j = curpos + i;
      for (std::size_t chan = 0; chan < Channels; chan++)
      {
        data[chan][cur_j] = convert_sample<SampleFormat, SampleSize>(dat[j]);
        j++;
      }
      i++;
    }
  }
};

template <typename SampleFormat, std::size_t SampleSize>
struct Decoder<SampleFormat, dynamic_channels, SampleSize, true>
{
  std::size_t Channels{};
  void operator()(audio_array& data, std::size_t curpos, uint8_t** buf, std::size_t n)
  {
    auto dat = reinterpret_cast<SampleFormat**>(buf);
    for (std::size_t chan = 0; chan < Channels; chan++)
    {
      for (std::size_t j = 0; j < n; j++)
      {
        data[chan][curpos + j] = convert_sample<SampleFormat, SampleSize>(dat[chan][j]);
      }
    }
  }
};

using decoder_t = eggs::variant<
    Decoder<int16_t, 1, 16, true>,
    Decoder<int16_t, 2, 16, true>,
    Decoder<int16_t, 2, 16, false>, /*
     Decoder<int16_t, 4, 16, true>,
     Decoder<int16_t, 4, 16, false>,
     Decoder<int16_t, 6, 16, true>,
     Decoder<int16_t, 6, 16, false>,
     Decoder<int16_t, 8, 16, true>,
     Decoder<int16_t, 8, 16, false>,*/
    Decoder<int16_t, dynamic_channels, 16, true>,
    Decoder<int16_t, dynamic_channels, 16, false>,

    Decoder<int32_t, 1, 24, true>,
    Decoder<int32_t, 2, 24, true>,
    Decoder<int32_t, 2, 24, false>, /*
     Decoder<int32_t, 4, 24, true>,
     Decoder<int32_t, 4, 24, false>,
     Decoder<int32_t, 6, 24, true>,
     Decoder<int32_t, 6, 24, false>,
     Decoder<int32_t, 8, 24, true>,
     Decoder<int32_t, 8, 24, false>,*/
    Decoder<int32_t, dynamic_channels, 24, true>,
    Decoder<int32_t, dynamic_channels, 24, false>,

    Decoder<int32_t, 1, 32, true>,
    Decoder<int32_t, 2, 32, true>,
    Decoder<int32_t, 2, 32, false>, /*
     Decoder<int32_t, 4, 32, true>,
     Decoder<int32_t, 4, 32, false>,
     Decoder<int32_t, 6, 32, true>,
     Decoder<int32_t, 6, 32, false>,
     Decoder<int32_t, 8, 32, true>,
     Decoder<int32_t, 8, 32, false>,*/
    Decoder<int32_t, dynamic_channels, 32, true>,
    Decoder<int32_t, dynamic_channels, 32, false>,

    Decoder<float, 1, 32, true>,
    Decoder<float, 2, 32, true>,
    Decoder<float, 2, 32, false>, /*
   Decoder<float,   4, 32, true>,
   Decoder<float,   4, 32, false>,
   Decoder<float,   6, 32, true>,
   Decoder<float,   6, 32, false>,
   Decoder<float,   8, 32, true>,
   Decoder<float,   8, 32, false>,*/
    Decoder<float, dynamic_channels, 32, true>,
    Decoder<float, dynamic_channels, 32, false>,

    Decoder<double, 1, 64, true>,
    Decoder<double, 2, 64, true>,
    Decoder<double, 2, 64, false>, /*
       Decoder<float,   4, 32, true>,
       Decoder<float,   4, 32, false>,
       Decoder<float,   6, 32, true>,
       Decoder<float,   6, 32, false>,
       Decoder<float,   8, 32, true>,
       Decoder<float,   8, 32, false>,*/
    Decoder<double, dynamic_channels, 64, true>,
    Decoder<double, dynamic_channels, 64, false>

    >;

template <std::size_t N>
decoder_t make_N_decoder(AVStream& stream)
{
  const int size = stream.codecpar->bits_per_raw_sample;

  if (size == 0 || size == 16 || size == 32)
  {
    switch ((AVSampleFormat)stream.codecpar->format)
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
  else if (size == 24)
  {
    switch ((AVSampleFormat)stream.codecpar->format)
    {
      case AVSampleFormat::AV_SAMPLE_FMT_S32:
        return Decoder<int32_t, N, 24, false>{};
      case AVSampleFormat::AV_SAMPLE_FMT_S32P:
        return Decoder<int32_t, N, 24, true>{};
      default:
        break;
    }
  }
  else if (size == 64)
  {
    switch ((AVSampleFormat)stream.codecpar->format)
    {
      case AVSampleFormat::AV_SAMPLE_FMT_DBL:
        return Decoder<double, N, 64, false>{};
      case AVSampleFormat::AV_SAMPLE_FMT_DBLP:
        return Decoder<double, N, 64, true>{};
      default:
        return {};
    }
  }
  return {};
}

decoder_t make_dynamic_decoder(AVStream& stream)
{
  const int size = stream.codecpar->bits_per_raw_sample;
  const std::size_t channels = stream.codecpar->channels;

  if (size == 0 || size == 16 || size == 32)
  {
    switch ((AVSampleFormat)stream.codecpar->format)
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
  else if (size == 24)
  {
    switch ((AVSampleFormat)stream.codecpar->format)
    {
      case AVSampleFormat::AV_SAMPLE_FMT_S32:
        return Decoder<int32_t, dynamic_channels, 24, false>{channels};
      case AVSampleFormat::AV_SAMPLE_FMT_S32P:
        return Decoder<int32_t, dynamic_channels, 24, true>{channels};
      default:
        break;
    }
  }
  else if (size == 64)
  {
    switch ((AVSampleFormat)stream.codecpar->format)
    {
      case AVSampleFormat::AV_SAMPLE_FMT_DBL:
        return Decoder<double, dynamic_channels, 64, false>{};
      case AVSampleFormat::AV_SAMPLE_FMT_DBLP:
        return Decoder<double, dynamic_channels, 64, true>{};
      default:
        return {};
    }
  }

  return {};
}

// Simple mono case: everything is planar
template <>
decoder_t make_N_decoder<1>(AVStream& stream)
{
  const int size = stream.codecpar->bits_per_raw_sample;

  if (size == 0 || size == 16 || size == 32)
  {
    switch ((AVSampleFormat)stream.codecpar->format)
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
  else if (size == 24)
  {
    return Decoder<int32_t, 1, 24, true>{};
  }
  else if (size == 64)
  {
    return Decoder<double, 1, 64, true>{};
  }

  return {};
}

static decoder_t make_decoder(AVStream& stream)
{
  switch (stream.codecpar->channels)
  {
    case 1:
      return make_N_decoder<1>(stream);
    case 2:
      return make_N_decoder<2>(stream); /*
case 4: return make_N_decoder<4>(stream);
case 6: return make_N_decoder<6>(stream);
case 8: return make_N_decoder<8>(stream);*/
    case 0:
      return {};
    default:
      return make_dynamic_decoder(stream);
  }
}
}
#endif

struct AVCodecContext;
struct AVFormatContext;
struct AVFrame;
namespace Media
{
AudioDecoder::AudioDecoder(int rate) : convertedSampleRate{rate}
{
  connect(
      this, &AudioDecoder::startDecode, this, &AudioDecoder::on_startDecode, Qt::QueuedConnection);
}

AudioDecoder::~AudioDecoder()
{
  m_decodeThread.exit();
  while (m_decodeThread.isRunning())
    ;
}

struct AVCodecContext_Free
{
  void operator()(AVCodecContext* ctx) const noexcept
  {
#if SCORE_HAS_LIBAV
    avcodec_free_context(&ctx);
#endif
  }
};
struct AVFormatContext_Free
{
  void operator()(AVFormatContext* ctx) const noexcept
  {
#if SCORE_HAS_LIBAV
    avformat_close_input(&ctx);
#endif
  }
};
struct AVFrame_Free
{
  void operator()(AVFrame* frame) const noexcept
  {
#if SCORE_HAS_LIBAV
    av_frame_free(&frame);
#endif
  }
};
using AVFormatContext_ptr = std::unique_ptr<AVFormatContext, AVFormatContext_Free>;
using AVCodecContext_ptr = std::unique_ptr<AVCodecContext, AVCodecContext_Free>;
using AVFrame_ptr = std::unique_ptr<AVFrame, AVFrame_Free>;

AVFormatContext_ptr open_audio(const QString& path)
{
#if SCORE_HAS_LIBAV
  AVFormatContext* fmt_ctx_ptr{};
  auto l1 = path.toUtf8();
  auto ret = avformat_open_input(&fmt_ctx_ptr, l1.constData(), nullptr, nullptr);
  if (ret != 0)
  {
    char err[100]{0};
    av_make_error_string(err, 100, ret);
    throw std::runtime_error(
        "Couldn't open file: " + std::string(l1.constData()) + " => " + std::string(err));
  }

  return AVFormatContext_ptr{fmt_ctx_ptr};
#else
  return {};
#endif
}

std::optional<AudioInfo> AudioDecoder::probe(const QString& path)
{
  auto it = database().find(path);
  if (it == database().end())
  {
#if SCORE_HAS_LIBAV
    auto fmt_ctx = open_audio(path);

    if (avformat_find_stream_info(fmt_ctx.get(), nullptr) < 0)
      return {};

    for (std::size_t i = 0; i < fmt_ctx->nb_streams; i++)
    {
      if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
      {
        auto stream = fmt_ctx->streams[i];
        AudioInfo info;
        info.channels = stream->codecpar->channels;
        if (info.channels == 0)
          return {};
        info.rate = stream->codecpar->sample_rate;
        info.length = std::ceil(info.rate * read_length(path));
        info.max_arr_length = info.length;

        /*
        if (info.rate != m_targetSampleRate)
        {
          info.length
              = av_rescale_rnd(info.length, m_targetSampleRate, info.rate,
        AV_ROUND_UP);

          if (info.length > info.max_arr_length)
            info.max_arr_length = info.length;
        }
        */

        database().insert(path, info);
        return info;
      }
    }
#endif
    return {};
  }
  else
  {
    return *it;
  }
}

QHash<QString, AudioInfo>& AudioDecoder::database()
{
  static QHash<QString, AudioInfo> db;
  return db;
}

auto debug_ffmpeg(int ret, QString ctx)
{
#if SCORE_HAS_LIBAV
  if (ret < 0 && ret != AVERROR_EOF)
  {
    char err[100]{0};
    av_make_error_string(err, 100, ret);
    qDebug() << "error when " << ctx << ": " << ret << err;
  }
  else
  {
    // qDebug()<<"ok: "<< ctx ;
  }
#endif
}

double AudioDecoder::read_length(const QString& path)
{
#if SCORE_HAS_LIBAV
  av_register_all();
  avcodec_register_all();

  auto fmt_ctx = open_audio(path);

  auto ret = avformat_find_stream_info(fmt_ctx.get(), nullptr);
  if (ret != 0)
    throw std::runtime_error("Couldn't find stream information");

  return fmt_ctx->duration / double(AV_TIME_BASE);
#endif
  return 0;
}

void AudioDecoder::decode(const QString& path, audio_handle hdl)
{
#if SCORE_HAS_LIBAV
  SCORE_ASSERT(hdl);
  AudioInfo info;
  auto it = database().find(path);
  if (it == database().end())
  {
    try
    {
      if (auto res = probe(path))
        info = *res;
    }
    catch (...)
    {
      qDebug("Cannot decode without info");
      finishedDecoding(hdl);
      return;
    }
  }
  else
  {
    info = *it;
  }

  decoded = 0;
  fileSampleRate = info.rate;
  channels = info.channels;
  auto& data = hdl->data;
  data.resize(info.channels);

  if (convertedSampleRate != fileSampleRate)
    info.max_arr_length
        = av_rescale_rnd(info.max_arr_length, convertedSampleRate, fileSampleRate, AV_ROUND_UP);

  for (auto& c : data)
  {
    c.reserve(info.max_arr_length * 1.1);
    c.resize(info.max_arr_length);
  }

  if (data.size() == 0)
    return;

  this->moveToThread(&m_decodeThread);
  m_decodeThread.start();
  startDecode(path, hdl);
#endif
}

std::optional<std::pair<AudioInfo, audio_array>>
AudioDecoder::decode_synchronous(const QString& path, int rate)
{

  AudioDecoder dec(rate);
  auto res = dec.probe(path);
  if (!res)
    return std::nullopt;

  dec.fileSampleRate = res->rate;
  dec.channels = res->channels;

  auto hdl = std::make_shared<ossia::audio_data>();
  hdl->data.resize(res->channels);
  for (auto& c : hdl->data)
  {
    c.reserve(res->max_arr_length * 1.1);
    c.resize(res->max_arr_length);
  }

  dec.on_startDecode(path, hdl);

  return std::make_pair(*std::move(res), std::move(hdl->data));
}

template <typename Decoder>
void AudioDecoder::decodeFrame(Decoder dec, audio_array& data, AVFrame& frame)
{
#if SCORE_HAS_LIBAV
  const std::size_t channels = data.size();
  const std::size_t max_samples = data[0].size();

  if (convertedSampleRate != fileSampleRate)
  {
    auto new_len = av_rescale_rnd(frame.nb_samples, convertedSampleRate, fileSampleRate, AV_ROUND_UP);

    if (decoded + new_len > max_samples)
    {
      qDebug() << "ERROR" << decoded + frame.nb_samples << ">" << max_samples;
      return;
    }

    audio_array tmp;
    tmp.resize(channels);
    for (auto& sub : tmp)
      sub.resize(frame.nb_samples * 2);

    dec(tmp, 0, frame.extended_data, frame.nb_samples);

    int res = 0;
    for (std::size_t i = 0; i < channels; ++i)
    {
      audio_sample* out_ptr = data[i].data() + decoded;
      audio_sample* in_ptr = tmp[i].data();

      res = swr_convert(
          resampler[i], (uint8_t**)&out_ptr, new_len, (const uint8_t**)&in_ptr, frame.nb_samples);
    }
    decoded += res;
  }
  else
  {
    if (decoded + frame.nb_samples > max_samples)
    {
      qDebug() << "ERROR" << decoded + frame.nb_samples << ">" << max_samples;
      return;
    }

    dec(data, decoded, frame.extended_data, frame.nb_samples);
    decoded += frame.nb_samples;
  }
#endif
}

template <typename Decoder>
void AudioDecoder::decodeRemaining(Decoder dec, audio_array& data, AVFrame& frame)
{
#if SCORE_HAS_LIBAV
  const std::size_t channels = data.size();
  if (convertedSampleRate != fileSampleRate)
  {
    int res = 0;
    for (std::size_t i = 0; i < channels; ++i)
    {
      audio_sample* out_ptr = data[i].data() + decoded;

      res = swr_convert(resampler[i], (uint8_t**)&out_ptr, data[i].size() - decoded, nullptr, 0);
    }
    decoded += res;
  }
  else
  {
    /*
        const std::size_t channels = data.size();
        const std::size_t max_samples = data[0].size();
        if (decoded + frame.nb_samples > max_samples)
        {
          qDebug() << "ERROR" << decoded + frame.nb_samples << ">" <<
       max_samples; return;
        }

        dec(data, decoded, frame.extended_data, frame.nb_samples);
        decoded += frame.nb_samples;
        */
  }

  // TODO it should be zeros, but check to be sure..
  if (decoded < data[0].size())
    decoded = data[0].size();
#endif
}
void AudioDecoder::on_startDecode(QString path, audio_handle hdl)
{
#if SCORE_HAS_LIBAV
  auto& data = hdl->data;
  try
  {
    const std::size_t channels = data.size();

    av_register_all();
    avcodec_register_all();

    auto fmt_ctx = open_audio(path);

    auto ret = avformat_find_stream_info(fmt_ctx.get(), nullptr);
    if (ret != 0)
      throw std::runtime_error("Couldn't find stream information");

    AVStream* stream{};

    for (std::size_t i = 0; i < fmt_ctx->nb_streams; i++)
    {
      if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
      {
        stream = fmt_ctx->streams[i];
        break;
      }
    }

    if (!stream)
      throw std::runtime_error("Couldn't find any audio stream");

    auto codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!codec)
      throw std::runtime_error("Couldn't find codec");

    AVCodecContext_ptr codec_ctx{avcodec_alloc_context3(codec)};
    if (!codec_ctx)
      throw std::runtime_error("Couldn't allocate codec context");

    ret = avcodec_parameters_to_context(codec_ctx.get(), stream->codecpar);
    if (ret != 0)
      throw std::runtime_error("Couldn't copy codec data");

    ret = avcodec_open2(codec_ctx.get(), codec, nullptr);
    if (ret != 0)
      throw std::runtime_error("Couldn't open codec");

    auto decoder = make_decoder(*stream);
    if (!decoder)
      throw std::runtime_error("Couldn't create decoder");

    // init resampling
    if(convertedSampleRate != fileSampleRate)
    {
      for (std::size_t i = 0; i < channels; ++i)
      {
        SwrContext* swr = swr_alloc_set_opts(
            nullptr,
            AV_CH_LAYOUT_MONO,
            AV_SAMPLE_FMT_FLT,
            convertedSampleRate,
            AV_CH_LAYOUT_MONO,
            AV_SAMPLE_FMT_FLT,
            fileSampleRate,
            0,
            nullptr);
        swr_init(swr);
        resampler.push_back(swr);
      }
    }

    // decoding
    eggs::variants::apply(
        [&](auto& dec) {
          AVPacket packet;
          AVFrame_ptr frame{av_frame_alloc()};

          ret = av_read_frame(fmt_ctx.get(), &packet);

          debug_ffmpeg(ret, "av_read_frame");
          int update = 0;
          while (ret >= 0)
          {
            ret = avcodec_send_packet(codec_ctx.get(), &packet);
            debug_ffmpeg(ret, "avcodec_send_packet");
            if (ret == 0)
            {
              ret = avcodec_receive_frame(codec_ctx.get(), frame.get());
              debug_ffmpeg(ret, "avcodec_receive_frame");
              if (ret == 0)
              {
                while (ret == 0)
                {
                  decodeFrame(dec, data, *frame);
                  ret = avcodec_receive_frame(codec_ctx.get(), frame.get());

                  update++;
                  if ((update % 512) == 0)
                  {
                    newData();
                  }
                }

                ret = av_read_frame(fmt_ctx.get(), &packet);
                debug_ffmpeg(ret, "av_read_frame");
                continue;
              }
              else if (ret == AVERROR(EAGAIN))
              {
                ret = av_read_frame(fmt_ctx.get(), &packet);
                debug_ffmpeg(ret, "av_read_frame");
                continue;
              }
              else if (ret == AVERROR_EOF)
              {
                decodeFrame(dec, data, *frame);
                break;
              }
              else
              {
                break;
              }
            }
            else if (ret == AVERROR(EAGAIN))
            {
              ret = avcodec_receive_frame(codec_ctx.get(), frame.get());
              debug_ffmpeg(ret, "avcodec_receive_frame EAGAIN");
            }
            else
            {
              break;
            }
          }

          // Flush
          ret = avcodec_send_packet(codec_ctx.get(), nullptr);

          decodeRemaining(dec, data, *frame);
          newData();
        },
        decoder);

    // clear resampling
    for (auto swr : resampler)
      swr_free(&swr);
    resampler.clear();
  }
  catch (std::exception& e)
  {
    qDebug() << "Decoder error: " << e.what();
  }

  finishedDecoding(hdl);
  m_decodeThread.quit();

#endif
  return;
}
}
