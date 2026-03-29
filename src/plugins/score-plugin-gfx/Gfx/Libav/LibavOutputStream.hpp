#pragma once

extern "C" {

#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

#include <Audio/Settings/Model.hpp>
#include <Gfx/Libav/AudioFrameEncoder.hpp>
#include <Gfx/Libav/LibavOutputSettings.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/tools/Debug.hpp>

#include <ossia/detail/flat_map.hpp>

#include <QApplication>

#include <CDSPResampler.h>

#include <string>

namespace Gfx
{

struct StreamOptions
{
  std::string name;
  std::string codec;
  ossia::flat_map<std::string, std::string> options;
};

struct OutputStream
{
  const AVCodec* codec{};
  AVStream* st{};
  AVCodecContext* enc{};

  /* pts of the next frame that will be generated */
  int64_t next_pts{};
  int samples_count{};

  AVFrame* cache_input_frame{};
  AVFrame* tmp_frame{};

  AVPacket* tmp_pkt{};

  struct SwsContext* sws_ctx{};
  std::vector<std::unique_ptr<r8b::CDSPResampler>> resamplers;

  std::unique_ptr<AudioFrameEncoder> encoder;

  // Pre-allocated buffers for audio resampling (avoid per-frame heap allocs)
  std::vector<std::vector<double>> resample_in_buf;
  std::vector<ossia::float_vector> resample_out_buf;

  bool m_valid{};

  OutputStream(
      const LibavOutputSettings& set, AVFormatContext* oc, const StreamOptions& opts)
  {
    codec = avcodec_find_encoder_by_name(opts.codec.c_str());
    if(!codec)
    {
      qDebug() << "Could not find encoder for " << opts.codec.c_str();
      return;
    }

    this->tmp_pkt = av_packet_alloc();
    if(!this->tmp_pkt)
    {
      qDebug() << "Could not allocate AVPacket";
      return;
    }

    this->st = avformat_new_stream(oc, nullptr);
    if(!this->st)
    {
      qDebug() << "Could not allocate stream";
      return;
    }
    this->st->id = oc->nb_streams - 1;

    this->enc = avcodec_alloc_context3(codec);
    if(!this->enc)
    {
      qDebug() << "Could not alloc an encoding context";
      return;
    }

    switch(codec->type)
    {
      case AVMEDIA_TYPE_AUDIO:
        init_audio(set, this->enc);
        break;
      case AVMEDIA_TYPE_VIDEO:
        init_video(set, this->enc);
        break;

      default:
        break;
    }

    /* Some formats want stream headers to be separate. */
    if(oc->oformat->flags & AVFMT_GLOBALHEADER)
    {
      this->enc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }
  }

  void init_audio(const LibavOutputSettings& set, AVCodecContext* c)
  {
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 24, 100)
    c->sample_fmt = av_get_sample_fmt(set.audio_converted_smpfmt.toStdString().c_str());

    {
      const int* supported_samplerates{};
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(61, 19, 100)
      avcodec_get_supported_config(
          c, codec, AV_CODEC_CONFIG_SAMPLE_RATE, 0, (const void**)&supported_samplerates,
          nullptr);
#else
      supported_samplerates = codec->supported_samplerates;
#endif
      if(supported_samplerates)
      {
        c->sample_rate = supported_samplerates[0];
        for(int i = 0; supported_samplerates[i]; i++)
        {
          if(supported_samplerates[i] == set.audio_sample_rate)
          {
            c->sample_rate = set.audio_sample_rate;
            break;
          }
        }
      }
      else
      {
        c->sample_rate = set.audio_sample_rate;
      }
    }

    c->ch_layout.order = AV_CHANNEL_ORDER_UNSPEC;
    c->ch_layout.nb_channels = set.audio_channels;
    c->thread_count = set.threads > 0 ? set.threads : 0;
    if(set.audio_encoder_short == "pcm_s24le" || set.audio_encoder_short == "pcm_s24be")
      c->bits_per_raw_sample = 24;

    this->st->time_base = AVRational{1, c->sample_rate};
    c->time_base = AVRational{1, c->sample_rate};
    c->framerate = AVRational{c->sample_rate, 1};
    qDebug() << "Opening audio encoder with: rate: " << c->sample_rate;
#endif
  }

  void init_video(const LibavOutputSettings& set, AVCodecContext* c)
  {
    c->codec_id = codec->id;
    c->width = set.width;
    c->height = set.height;

    // Enable multi-threaded encoding (0 = auto-detect CPU count)
    c->thread_count = set.threads > 0 ? set.threads : 0;
    c->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;
    /* timebase: This is the fundamental unit of time (in seconds) in terms
         * of which frame timestamps are represented. For fixed-fps content,
         * timebase should be 1/framerate and timestamp increments should be
         * identical to 1. */
    this->st->time_base = AVRational{100000, int(100000 * set.rate)};
    c->time_base = this->st->time_base;
    c->framerate = AVRational{this->st->time_base.den, this->st->time_base.num};

    // gop_size and max_b_frames: use FFmpeg/codec defaults.
    // Users can override via the options dict (g=<N>, bf=<N>).
    // FFmpeg default: g=12, bf=0. Presets set explicit values where needed.

    c->pix_fmt = av_get_pix_fmt(set.video_converted_pixfmt.toStdString().c_str());
    if(c->pix_fmt == AV_PIX_FMT_NONE)
    {
      // Default to first supported format of this codec
      const AVPixelFormat* fmts = nullptr;
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(61, 19, 100)
      avcodec_get_supported_config(
          c, codec, AV_CODEC_CONFIG_PIX_FORMAT, 0,
          (const void**)&fmts, nullptr);
#else
      fmts = codec->pix_fmts;
#endif
      if(fmts && fmts[0] != AV_PIX_FMT_NONE)
        c->pix_fmt = fmts[0];
      else
        c->pix_fmt = AV_PIX_FMT_YUV420P;
    }
    c->strict_std_compliance = FF_COMPLIANCE_NORMAL;
  }

  void open_audio(
      const LibavOutputSettings& set, AVFormatContext* oc, const AVCodec* codec,
      AVDictionary* opt_arg)
  {
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 24, 100)
    AVDictionary* opt = nullptr;

    av_dict_copy(&opt, opt_arg, 0);
    int ret = avcodec_open2(enc, codec, &opt);
    av_dict_free(&opt);
    if(ret < 0)
    {
      qDebug() << "Could not open audio codec: " << av_to_string(ret);
      return;
    }

    int nb_samples = 0;
    if(enc->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE)
    {
      auto& audio_stgs = score::AppContext().settings<Audio::Settings::Model>();
      nb_samples = audio_stgs.getBufferSize();
      enc->frame_size = nb_samples;
      qDebug() << "Setting frame_size: " << nb_samples;
    }
    else
    {
      nb_samples = enc->frame_size;
      qDebug() << "Forcing frame_size: " << nb_samples;
    }

    cache_input_frame = alloc_audio_frame(
        enc->sample_fmt, &enc->ch_layout, enc->sample_rate, nb_samples);

    /* copy the stream parameters to the muxer */
    ret = avcodec_parameters_from_context(this->st->codecpar, enc);
    if(ret < 0)
    {
      qDebug() << "Could not copy the stream parameters";
      return;
    }

    {
      auto conv_fmt
          = av_get_sample_fmt(set.audio_converted_smpfmt.toStdString().c_str());
      if(conv_fmt == AV_SAMPLE_FMT_NONE)
      {
        qDebug() << "Invalid audio sample format:" << set.audio_converted_smpfmt;
        return;
      }

      auto& ctx = score::AppContext().settings<Audio::Settings::Model>();

      const int input_sample_rate = ctx.getRate();
      if(enc->sample_rate != input_sample_rate)
      {
        for(int i = 0; i < set.audio_channels; i++)
          this->resamplers.push_back(std::make_unique<r8b::CDSPResampler>(
              input_sample_rate, enc->sample_rate, nb_samples * 2, 3.0, 206.91,
              r8b::fprMinPhase));
      }

      switch(conv_fmt)
      {
        case AV_SAMPLE_FMT_NONE:
        case AV_SAMPLE_FMT_U8:
        case AV_SAMPLE_FMT_S16:
          encoder = std::make_unique<S16IAudioFrameEncoder>(nb_samples);
          break;
        case AV_SAMPLE_FMT_S32:
          if(enc->bits_per_raw_sample == 24)
            encoder = std::make_unique<S24IAudioFrameEncoder>(nb_samples);
          else
            encoder = std::make_unique<S32IAudioFrameEncoder>(nb_samples);
          break;
        case AV_SAMPLE_FMT_FLT:
          encoder = std::make_unique<FltIAudioFrameEncoder>(nb_samples);
          break;
        case AV_SAMPLE_FMT_DBL:
          encoder = std::make_unique<DblIAudioFrameEncoder>(nb_samples);
          break;

        case AV_SAMPLE_FMT_U8P:
        case AV_SAMPLE_FMT_S16P:
          encoder = std::make_unique<S16PAudioFrameEncoder>(nb_samples);
          break;
        case AV_SAMPLE_FMT_S32P:
          encoder = std::make_unique<S32PAudioFrameEncoder>(nb_samples);
          break;
        case AV_SAMPLE_FMT_FLTP:
          encoder = std::make_unique<FltPAudioFrameEncoder>(nb_samples);
          break;
        case AV_SAMPLE_FMT_DBLP:
          encoder = std::make_unique<DblPAudioFrameEncoder>(nb_samples);
          break;
        case AV_SAMPLE_FMT_S64:
        case AV_SAMPLE_FMT_S64P:
          qDebug() << "64-bit integer audio sample format not supported for encoding";
          break;
        default:
          break;
      }
    }

    m_valid = true;
#endif
  }

#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 24, 100)
  static AVFrame* alloc_audio_frame(
      enum AVSampleFormat sample_fmt, const AVChannelLayout* channel_layout,
      int sample_rate, int nb_samples)
  {
    AVFrame* frame = av_frame_alloc();
    if(!frame)
    {
      qDebug() << "Error allocating an audio frame";
      return nullptr;
    }

    frame->format = sample_fmt;
    av_channel_layout_copy(&frame->ch_layout, channel_layout);
    frame->sample_rate = sample_rate;
    frame->nb_samples = nb_samples;

    if(nb_samples)
    {
      if(av_frame_get_buffer(frame, 0) < 0)
      {
        qDebug() << "Error allocating an audio buffer";
        av_frame_free(&frame);
        return nullptr;
      }
    }

    return frame;
  }
#endif

  static AVFrame* alloc_video_frame(enum AVPixelFormat pix_fmt, int width, int height)
  {
    auto frame = av_frame_alloc();
    if(!frame)
      return NULL;

    frame->format = pix_fmt;
    frame->width = width;
    frame->height = height;

    /* allocate the buffers for the frame data */
    const int ret = av_frame_get_buffer(frame, 0);
    if(ret < 0)
    {
      qDebug() << "Could not allocate frame data.";
      av_frame_free(&frame);
      return nullptr;
    }

    return frame;
  }

  void open_video(
      const LibavOutputSettings& set, AVFormatContext* oc, const AVCodec* codec,
      AVDictionary* opt_arg)
  {
    AVCodecContext* c = this->enc;
    AVDictionary* opt = nullptr;

    av_dict_copy(&opt, opt_arg, 0);

    /* open the codec */
    int ret = avcodec_open2(this->enc, codec, &opt);
    av_dict_free(&opt);
    if(ret < 0)
    {
      qDebug() << "Could not open video codec: " << av_to_string(ret);
      return;
    }

    /* allocate and init a re-usable frame */
    this->cache_input_frame = alloc_video_frame(AV_PIX_FMT_RGBA, c->width, c->height);
    if(!this->cache_input_frame)
    {
      qDebug() << "Could not allocate video frame";
      return;
    }

    this->tmp_frame = nullptr;
    {
      auto input_fmt = av_get_pix_fmt(set.video_render_pixfmt.toStdString().c_str());
      auto conv_fmt = av_get_pix_fmt(set.video_converted_pixfmt.toStdString().c_str());
      if(input_fmt == AV_PIX_FMT_NONE || conv_fmt == AV_PIX_FMT_NONE)
      {
        qDebug() << "Invalid pixel format:" << set.video_render_pixfmt
                 << "->" << set.video_converted_pixfmt;
        return;
      }
      sws_ctx = sws_getContext(
          set.width, set.height, input_fmt, set.width, set.height, conv_fmt,
          SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
      if(!sws_ctx)
      {
        qDebug() << "Could not create swscale context";
        return;
      }
      this->tmp_frame = alloc_video_frame(conv_fmt, c->width, c->height);
      if(!this->tmp_frame)
      {
        qDebug() << "Could not allocate temporary video frame";
        return;
      }
    }

    /* copy the stream parameters to the muxer */
    ret = avcodec_parameters_from_context(this->st->codecpar, c);
    if(ret < 0)
    {
      qDebug() << "Could not copy the stream parameters";
      return;
    }

    m_valid = true;
  }

  void open(
      const LibavOutputSettings& set, AVFormatContext* oc, const AVCodec* codec,
      AVDictionary* opt_arg)
  {
    SCORE_ASSERT(oc);
    SCORE_ASSERT(codec);
    if(codec->type == AVMEDIA_TYPE_AUDIO)
    {
      open_audio(set, oc, codec, opt_arg);
    }
    else if(codec->type == AVMEDIA_TYPE_VIDEO)
    {
      open_video(set, oc, codec, opt_arg);
    }
  }

  void close(AVFormatContext* oc)
  {
    avcodec_free_context(&enc);
    av_frame_free(&cache_input_frame);
    av_frame_free(&tmp_frame);
    av_packet_free(&tmp_pkt);
    sws_freeContext(sws_ctx);
    sws_ctx = nullptr;
  }

  AVFrame* get_video_frame()
  {
    if(!m_valid || !this->cache_input_frame)
      return nullptr;
    if(av_frame_make_writable(this->cache_input_frame) < 0)
      return nullptr;

    this->cache_input_frame->pts = this->next_pts++;
    return this->cache_input_frame;
  }

  AVFrame* get_audio_frame()
  {
    if(!m_valid || !this->cache_input_frame)
      return nullptr;
    if(av_frame_make_writable(this->cache_input_frame) < 0)
      return nullptr;

    this->cache_input_frame->pts = this->next_pts;
    this->next_pts += this->enc->frame_size;
    return this->cache_input_frame;
  }

  int write_video_frame(AVFormatContext* fmt_ctx, AVFrame* input_frame)
  {
    if(!m_valid)
      return -1;
#if LIBSWSCALE_VERSION_INT >= AV_VERSION_INT(7, 5, 100)
    // Must unref before reuse — sws_scale_frame allocates internal buffers
    av_frame_unref(tmp_frame);
    tmp_frame->format = enc->pix_fmt;
    tmp_frame->width = enc->width;
    tmp_frame->height = enc->height;

    // scale the frame
    int ret = sws_scale_frame(sws_ctx, tmp_frame, input_frame);
    if(ret < 0)
    {
      qDebug() << "Error during sws_scale_frame: " << av_to_string(ret);
      return ret;
    }

    tmp_frame->pts = input_frame->pts;

    // send the frame to the encoder
    ret = avcodec_send_frame(enc, tmp_frame);
    if(ret < 0)
    {
      qDebug() << "Error sending a frame to the encoder: " << av_to_string(ret);
      return ret;
    }

    while(ret >= 0)
    {
      ret = avcodec_receive_packet(enc, tmp_pkt);
      if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        break;
      else if(ret < 0)
      {
        qDebug() << "Error encoding a frame: " << av_to_string(ret);
        return ret;
      }

      /* rescale output packet timestamp values from codec to stream timebase */
      av_packet_rescale_ts(tmp_pkt, enc->time_base, st->time_base);
      tmp_pkt->stream_index = st->index;

      ret = av_interleaved_write_frame(fmt_ctx, tmp_pkt);
      if(ret < 0)
      {
        qDebug() << "Error while writing output packet: " << av_to_string(ret);
        return ret;
      }
    }

    return ret == AVERROR_EOF ? 1 : 0;
#endif
    return 1;
  }

  // Write a pre-converted frame directly to the encoder — no sws_scale.
  // The frame must already be in enc->pix_fmt with correct dimensions.
  int write_video_frame_direct(AVFormatContext* fmt_ctx, AVFrame* frame)
  {
    if(!m_valid)
      return -1;

    int ret = avcodec_send_frame(enc, frame);
    if(ret < 0)
    {
      qDebug() << "Error sending a frame to the encoder: " << av_to_string(ret);
      return ret;
    }

    while(ret >= 0)
    {
      ret = avcodec_receive_packet(enc, tmp_pkt);
      if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        break;
      else if(ret < 0)
      {
        qDebug() << "Error encoding a frame: " << av_to_string(ret);
        return ret;
      }

      av_packet_rescale_ts(tmp_pkt, enc->time_base, st->time_base);
      tmp_pkt->stream_index = st->index;

      ret = av_interleaved_write_frame(fmt_ctx, tmp_pkt);
      if(ret < 0)
      {
        qDebug() << "Error while writing output packet: " << av_to_string(ret);
        return ret;
      }
    }

    return ret == AVERROR_EOF ? 1 : 0;
  }

  int write_audio_frame(AVFormatContext* fmt_ctx, AVFrame* input_frame)
  {
    if(!m_valid)
      return -1;
    // send the frame to the encoder
    int ret = avcodec_send_frame(enc, input_frame);
    if(ret < 0)
    {
      qDebug() << "Error sending a frame to the encoder: " << av_to_string(ret);
      return ret;
    }

    while(ret >= 0)
    {
      ret = avcodec_receive_packet(enc, tmp_pkt);
      if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        break;
      else if(ret < 0)
      {
        qDebug() << "Error encoding a frame: " << av_to_string(ret);
        return ret;
      }

      /* rescale output packet timestamp values from codec to stream timebase */
      av_packet_rescale_ts(tmp_pkt, enc->time_base, st->time_base);
      tmp_pkt->stream_index = st->index;

      ret = av_interleaved_write_frame(fmt_ctx, tmp_pkt);
      if(ret < 0)
      {
        qDebug() << "Error while writing output packet: " << av_to_string(ret);
        return ret;
      }
    }

    return ret == AVERROR_EOF ? 1 : 0;
  }
};
}
