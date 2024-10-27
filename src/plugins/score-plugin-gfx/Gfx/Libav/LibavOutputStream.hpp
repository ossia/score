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

  OutputStream(
      const LibavOutputSettings& set, AVFormatContext* oc, const StreamOptions& opts)
  {
    codec = avcodec_find_encoder_by_name(opts.codec.c_str());
    if(!codec)
    {
      qDebug() << "Could not find encoder for " << opts.codec.c_str();
      exit(1);
    }

    this->tmp_pkt = av_packet_alloc();
    if(!this->tmp_pkt)
    {
      qDebug() << "Could not allocate AVPacket";
      exit(1);
    }

    this->st = avformat_new_stream(oc, nullptr);
    if(!this->st)
    {
      qDebug() << "Could not allocate stream";
      exit(1);
    }
    this->st->id = oc->nb_streams - 1;

    // Init hw accel
    // FIXME IMPLEMENT
#if 0
    AVBufferRef* hw_ctx{};
    {
      // HW Accel
      AVHWDeviceType device = AV_HWDEVICE_TYPE_QSV;
      int ret = av_hwdevice_ctx_create(&hw_ctx, device, "auto", nullptr, 0);
      if(ret != 0)
      {
        qDebug() << "Error while opening hardware encoder: " << av_to_string(ret);
        exit(1);
      }
    }
#endif
    this->enc = avcodec_alloc_context3(codec);
    if(!this->enc)
    {
      qDebug() << "Could not alloc an encoding context";
      exit(1);
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

    if(codec->supported_samplerates)
    {
      c->sample_rate = codec->supported_samplerates[0];
      for(int i = 0; codec->supported_samplerates[i]; i++)
      {
        if(codec->supported_samplerates[i] == set.audio_sample_rate)
          c->sample_rate = set.audio_sample_rate;
      }
    }
    else
    {
      c->sample_rate = set.audio_sample_rate;
    }

    c->ch_layout.order = AV_CHANNEL_ORDER_UNSPEC;
    c->ch_layout.nb_channels = set.audio_channels;
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
    // c->bit_rate = 400000;
    // c->bit_rate_tolerance = 10000;
    // c->global_quality = 1;
    // c->compression_level = 1;
    // c->hw_device_ctx = hw_ctx;

    // c->flags |= AV_CODEC_FLAG_QSCALE;
    // c->global_quality = FF_QP2LAMBDA * 3.0;
    /* Resolution must be a multiple of two. */
    c->width = set.width;
    c->height = set.height;
    /* timebase: This is the fundamental unit of time (in seconds) in terms
         * of which frame timestamps are represented. For fixed-fps content,
         * timebase should be 1/framerate and timestamp increments should be
         * identical to 1. */
    this->st->time_base = AVRational{100000, int(100000 * set.rate)};
    c->time_base = this->st->time_base;
    c->framerate = AVRational{this->st->time_base.den, this->st->time_base.num};

    //c->gop_size = 12; /* emit one intra frame every twelve frames at most */

    // ignored if frame->pict_type is AV_PICTURE_TYPE_I
    c->gop_size = 0;
    c->max_b_frames = 0;

    // c->pix_fmt = AV_PIX_FMT_RGB24;
    c->pix_fmt = av_get_pix_fmt(set.video_converted_pixfmt.toStdString().c_str());
    c->strict_std_compliance = FF_COMPLIANCE_NORMAL;
    if(c->codec_id == AV_CODEC_ID_MPEG2VIDEO)
    {
      /* just for testing, we also add B-frames */
      c->max_b_frames = 2;
    }
    if(c->codec_id == AV_CODEC_ID_MPEG1VIDEO)
    {
      /* Needed to avoid using macroblocks in which some coeffs overflow.
       * This does not happen with normal video, it just happens here as
       * the motion of the chroma plane does not match the luma plane. */
      c->mb_decision = 2;
    }
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
      exit(1);
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
      exit(1);
    }

    {
      auto input_fmt = AV_SAMPLE_FMT_FLTP;
      auto conv_fmt
          = av_get_sample_fmt(set.audio_converted_smpfmt.toStdString().c_str());
      SCORE_ASSERT(input_fmt != -1);
      SCORE_ASSERT(conv_fmt != -1);

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
        case AV_SAMPLE_FMT_S32P:
        case AV_SAMPLE_FMT_FLTP:
          encoder = std::make_unique<FltPAudioFrameEncoder>(nb_samples);
          break;
        case AV_SAMPLE_FMT_DBLP:
        case AV_SAMPLE_FMT_S64:
        case AV_SAMPLE_FMT_S64P:
          break;
        default:
          break;
      }
    }
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
      exit(1);
    }

    frame->format = sample_fmt;
    frame->ch_layout.order = channel_layout->order;
    frame->ch_layout.nb_channels = channel_layout->nb_channels;
    frame->sample_rate = sample_rate;
    frame->nb_samples = nb_samples;

    if(nb_samples)
    {
      if(av_frame_get_buffer(frame, 0) < 0)
      {
        qDebug() << "Error allocating an audio buffer";
        exit(1);
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
      exit(1);
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

    /* set some options */
    int err = av_opt_set_double(this->enc->priv_data, "crf", 0.0, 0);
    if(err < 0)
    {
      qDebug() << "failed to initialize encoder: " << av_to_string(err);
    }

    /* open the codec */
    SCORE_ASSERT(this->enc->flags & AV_CODEC_FLAG_GLOBAL_HEADER);
    int ret = avcodec_open2(this->enc, codec, &opt);
    av_dict_free(&opt);
    if(ret < 0)
    {
      qDebug() << "Could not open video codec: " << av_to_string(ret);
      exit(1);
    }

    /* allocate and init a re-usable frame */
    this->cache_input_frame = alloc_video_frame(AV_PIX_FMT_RGBA, c->width, c->height);
    if(!this->cache_input_frame)
    {
      qDebug() << "Could not allocate video frame";
      exit(1);
    }

    this->tmp_frame = nullptr;
    // If conversion is needed :
    // if(c->pix_fmt != AV_PIX_FMT_YUVJ420P)
    {
      auto input_fmt = av_get_pix_fmt(set.video_render_pixfmt.toStdString().c_str());
      auto conv_fmt = av_get_pix_fmt(set.video_converted_pixfmt.toStdString().c_str());
      SCORE_ASSERT(input_fmt != -1);
      SCORE_ASSERT(conv_fmt != -1);
      sws_ctx = sws_getContext(
          set.width, set.height, input_fmt, set.width, set.height, conv_fmt, 1, nullptr,
          nullptr, nullptr);
      SCORE_ASSERT(sws_ctx);
      this->tmp_frame = alloc_video_frame(conv_fmt, c->width, c->height);
      if(!this->tmp_frame)
      {
        qDebug() << "Could not allocate temporary video frame";
        exit(1);
      }
    }

    /* copy the stream parameters to the muxer */
    ret = avcodec_parameters_from_context(this->st->codecpar, c);
    if(ret < 0)
    {
      qDebug() << "Could not copy the stream parameters";
      exit(1);
    }
  }

  void open(
      const LibavOutputSettings& set, AVFormatContext* oc, const AVCodec* codec,
      AVDictionary* opt_arg)
  {
    SCORE_ASSERT(oc);
    SCORE_ASSERT(codec);
    SCORE_ASSERT(opt_arg);
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
    /* when we pass a frame to the encoder, it may keep a reference to it
     * internally; make sure we do not overwrite it here */
    if(av_frame_make_writable(this->cache_input_frame) < 0)
      exit(1);

    this->cache_input_frame->pts = this->next_pts++;

    return this->cache_input_frame;
  }

  AVFrame* get_audio_frame()
  {
    /* when we pass a frame to the encoder, it may keep a reference to it
     * internally; make sure we do not overwrite it here */
    if(av_frame_make_writable(this->cache_input_frame) < 0)
      exit(1);

    this->cache_input_frame->pts = this->next_pts;
    this->next_pts += this->enc->frame_size;

    return this->cache_input_frame;
  }

  int write_video_frame(AVFormatContext* fmt_ctx, AVFrame* input_frame)
  {
#if LIBSWSCALE_VERSION_INT >= AV_VERSION_INT(7, 5, 100)
    // scale the frame
    int ret = sws_scale_frame(sws_ctx, tmp_frame, input_frame);
    if(ret < 0)
    {
      qDebug() << "Error during sws_scale_frame: " << av_to_string(ret);
      exit(1);
    }

    tmp_frame->quality = FF_LAMBDA_MAX; //c->global_quality;
    tmp_frame->pict_type = AV_PICTURE_TYPE_I;
    tmp_frame->pts++;

    // send the frame to the encoder
    ret = avcodec_send_frame(enc, tmp_frame);
    if(ret < 0)
    {
      qDebug() << "Error sending a frame to the encoder: " << av_to_string(ret);
      exit(1);
    }

    while(ret >= 0)
    {
      ret = avcodec_receive_packet(enc, tmp_pkt);
      if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        break;
      else if(ret < 0)
      {
        qDebug() << "Error encoding a frame: " << av_to_string(ret);
        exit(1);
      }

      /* rescale output packet timestamp values from codec to stream timebase */
      av_packet_rescale_ts(tmp_pkt, enc->time_base, st->time_base);
      tmp_pkt->stream_index = st->index;
      tmp_pkt->flags |= AV_PKT_FLAG_KEY;

      ret = av_interleaved_write_frame(fmt_ctx, tmp_pkt);
      if(ret < 0)
      {
        qDebug() << "Error while writing output packet: " << av_to_string(ret);
        exit(1);
      }
    }

    return ret == AVERROR_EOF ? 1 : 0;
#endif
    return 1;
  }

  // #define SRC_RATE SAMPLE_RATE_TEST
  // #define DST_RATE SAMPLE_RATE_TEST
  //   static int64_t conv_audio_pts(SwrContext* ctx, int64_t in, int sample_rate)
  //   {
  //     //int64_t d = (int64_t) AUDIO_RATE * AUDIO_RATE;
  //     int64_t d = (int64_t)sample_rate * sample_rate;
  //
  //     /* Convert from audio_src_tb to 1/(src_samplerate * dst_samplerate) */
  //     in = av_rescale_rnd(in, d, SRC_RATE, AV_ROUND_NEAR_INF);
  //
  //     /* In units of 1/(src_samplerate * dst_samplerate) */
  //     in = swr_next_pts(ctx, in);
  //
  //     /* Convert from 1/(src_samplerate * dst_samplerate) to audio_dst_tb */
  //     return av_rescale_rnd(in, DST_RATE, d, AV_ROUND_NEAR_INF);
  //   }

  int write_audio_frame(AVFormatContext* fmt_ctx, AVFrame* input_frame)
  {
    // send the frame to the encoder
    int ret = avcodec_send_frame(enc, input_frame);
    if(ret < 0)
    {
      qDebug() << "Error sending a frame to the encoder: " << av_to_string(ret);
      exit(1);
    }

    while(ret >= 0)
    {
      ret = avcodec_receive_packet(enc, tmp_pkt);
      if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        break;
      else if(ret < 0)
      {
        qDebug() << "Error encoding a frame: " << av_to_string(ret);
        exit(1);
      }

      /* rescale output packet timestamp values from codec to stream timebase */
      av_packet_rescale_ts(tmp_pkt, enc->time_base, st->time_base);
      tmp_pkt->stream_index = st->index;

      ret = av_interleaved_write_frame(fmt_ctx, tmp_pkt);
      if(ret < 0)
      {
        qDebug() << "Error while writing output packet: " << av_to_string(ret);
        exit(1);
      }
    }

    return ret == AVERROR_EOF ? 1 : 0;
  }
};
}
