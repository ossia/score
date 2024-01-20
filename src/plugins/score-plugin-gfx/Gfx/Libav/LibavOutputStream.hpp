#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

#include <Gfx/Libav/LibavOutputSettings.hpp>

#include <score/tools/Debug.hpp>

#include <ossia/detail/flat_map.hpp>

#include <QApplication>

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

  AVFrame* frame{};
  AVFrame* tmp_frame{};

  AVPacket* tmp_pkt{};

  struct SwsContext* sws_ctx{};
  struct SwrContext* swr_ctx{};

  OutputStream(
      const LibavOutputSettings& set, AVFormatContext* oc, const StreamOptions& opts)
  {
    /* find the encoder */
    codec = avcodec_find_encoder_by_name(opts.codec.c_str());
    // codec = avcodec_find_encoder(opts.codec_id);
    if(!codec)
    {
      fprintf(stderr, "Could not find encoder for '%s'\n", opts.codec.c_str());
      exit(1);
    }

    this->tmp_pkt = av_packet_alloc();
    if(!this->tmp_pkt)
    {
      fprintf(stderr, "Could not allocate AVPacket\n");
      exit(1);
    }

    this->st = avformat_new_stream(oc, NULL);
    if(!this->st)
    {
      fprintf(stderr, "Could not allocate stream\n");
      exit(1);
    }
    this->st->id = oc->nb_streams - 1;

    // Init hw accel
    AVBufferRef* hw_ctx{};
#if 0
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
    AVCodecContext* c = avcodec_alloc_context3(codec);
    if(!c)
    {
      fprintf(stderr, "Could not alloc an encoding context\n");
      exit(1);
    }
    this->enc = c;

    switch(codec->type)
    {
      case AVMEDIA_TYPE_AUDIO: {
        c->sample_fmt = codec->sample_fmts ? codec->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
        c->bit_rate = 64000;
        c->sample_rate = 44100;
        if(codec->supported_samplerates)
        {
          c->sample_rate = codec->supported_samplerates[0];
          for(int i = 0; codec->supported_samplerates[i]; i++)
          {
            if(codec->supported_samplerates[i] == 44100)
              c->sample_rate = 44100;
          }
        }
        constexpr AVChannelLayout layout
            = {.order = AV_CHANNEL_ORDER_UNSPEC, .nb_channels = 2};
        av_channel_layout_copy(&c->ch_layout, &layout);
        this->st->time_base = (AVRational){1, c->sample_rate};
        break;
      }

      case AVMEDIA_TYPE_VIDEO: {
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
        this->st->time_base = (AVRational){100000, int(100000 * set.rate)};
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
        break;
      }

      default:
        break;
    }

    /* Some formats want stream headers to be separate. */
    if(oc->oformat->flags & AVFMT_GLOBALHEADER)
    {
      c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }
  }

  void open_audio(AVFormatContext* oc, const AVCodec* codec, AVDictionary* opt_arg)
  {
    AVCodecContext* c;
    int ret;
    AVDictionary* opt = NULL;

    c = this->enc;

    /* open it */
    av_dict_copy(&opt, opt_arg, 0);
    ret = avcodec_open2(c, codec, &opt);
    av_dict_free(&opt);
    if(ret < 0)
    {
      fprintf(stderr, "Could not open audio codec: %s\n", av_err2str(ret));
      exit(1);
    }

    // FIXME
    // int nb_samples;
    // if(c->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE)
    //   nb_samples = 10000;
    // else
    //   nb_samples = c->frame_size;
    //
    // ost->frame = alloc_audio_frame(c->sample_fmt, &c->ch_layout, c->sample_rate, nb_samples);
    // ost->tmp_frame = alloc_audio_frame(        AV_SAMPLE_FMT_S16, &c->ch_layout, c->sample_rate, nb_samples);

    /* copy the stream parameters to the muxer */
    ret = avcodec_parameters_from_context(this->st->codecpar, c);
    if(ret < 0)
    {
      fprintf(stderr, "Could not copy the stream parameters\n");
      exit(1);
    }

    /* create resampler context */
    {
      this->swr_ctx = swr_alloc();
      if(!this->swr_ctx)
      {
        fprintf(stderr, "Could not allocate resampler context\n");
        exit(1);
      }

      auto in_chlayout = c->ch_layout;
      auto out_chlayout = c->ch_layout;
      auto in_rate = c->sample_rate;
      auto out_rate = c->sample_rate;
      auto in_fmt = AV_SAMPLE_FMT_FLTP;
      auto out_fmt = c->sample_fmt;

      ret = swr_alloc_set_opts2(
          &this->swr_ctx, &out_chlayout, out_fmt, out_rate, &in_chlayout, in_fmt,
          in_rate, 0, nullptr);

      if(ret < 0)
      {
        fprintf(stderr, "Failed to initialize the resampling context\n");
        exit(1);
      }
    }
  }

  static AVFrame* alloc_frame(enum AVPixelFormat pix_fmt, int width, int height)
  {
    AVFrame* frame;
    int ret;

    frame = av_frame_alloc();
    if(!frame)
      return NULL;

    frame->format = pix_fmt;
    frame->width = width;
    frame->height = height;

    /* allocate the buffers for the frame data */
    ret = av_frame_get_buffer(frame, 0);
    if(ret < 0)
    {
      fprintf(stderr, "Could not allocate frame data.\n");
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
      qDebug() << "failed to initialize encoder: " << av_err2str(err);
    }

    /* open the codec */
    SCORE_ASSERT(this->enc->flags & AV_CODEC_FLAG_GLOBAL_HEADER);
    qDebug() << "Flagz: " << (int64_t)this->enc->flags << AV_CODEC_FLAG_GLOBAL_HEADER;
    int ret = avcodec_open2(this->enc, codec, &opt);
    av_dict_free(&opt);
    if(ret < 0)
    {
      fprintf(stderr, "Could not open video codec: %s\n", av_err2str(ret));
      exit(1);
    }

    /* allocate and init a re-usable frame */
    this->frame = alloc_frame(AV_PIX_FMT_RGBA, c->width, c->height);
    if(!this->frame)
    {
      fprintf(stderr, "Could not allocate video frame\n");
      exit(1);
    }

    this->tmp_frame = nullptr;
    // If conversion is needed :
    // if(c->pix_fmt != AV_PIX_FMT_YUVJ420P)
    {
      auto input_fmt = av_get_pix_fmt(set.video_input_pixfmt.toStdString().c_str());
      auto conv_fmt = av_get_pix_fmt(set.video_converted_pixfmt.toStdString().c_str());
      SCORE_ASSERT(input_fmt != -1);
      SCORE_ASSERT(conv_fmt != -1);
      sws_ctx = sws_getContext(
          set.width, set.height, input_fmt, set.width, set.height, conv_fmt, 1, nullptr,
          nullptr, nullptr);
      this->tmp_frame = alloc_frame(conv_fmt, c->width, c->height);
      if(!this->tmp_frame)
      {
        fprintf(stderr, "Could not allocate temporary video frame\n");
        exit(1);
      }
    }

    /* copy the stream parameters to the muxer */
    ret = avcodec_parameters_from_context(this->st->codecpar, c);
    if(ret < 0)
    {
      fprintf(stderr, "Could not copy the stream parameters\n");
      exit(1);
    }

    /* Copy extradata (e.g. H264 NALs) */
    qDebug() << c->extradata_size << this->st->codecpar->extradata_size;
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
      open_audio(oc, codec, opt_arg);
    }
    else if(codec->type == AVMEDIA_TYPE_VIDEO)
    {
      open_video(set, oc, codec, opt_arg);
    }
  }

  void close(AVFormatContext* oc)
  {
    avcodec_free_context(&enc);
    av_frame_free(&frame);
    av_frame_free(&tmp_frame);
    av_packet_free(&tmp_pkt);
    sws_freeContext(sws_ctx);
    swr_free(&swr_ctx);
  }

  AVFrame* get_video_frame()
  {
    /* when we pass a frame to the encoder, it may keep a reference to it
     * internally; make sure we do not overwrite it here */
    if(av_frame_make_writable(this->frame) < 0)
      exit(1);

    this->frame->pts = this->next_pts++;

    return this->frame;
  }

  int write_frame(
      AVFormatContext* fmt_ctx, AVCodecContext* c, AVStream* st, AVFrame* frame,
      AVPacket* pkt)
  {
    int ret;
    // scale the frame
    sws_scale_frame(sws_ctx, tmp_frame, frame);

    tmp_frame->quality = FF_LAMBDA_MAX; //c->global_quality;
    tmp_frame->pict_type = AV_PICTURE_TYPE_I;
    tmp_frame->pts++;

    // send the frame to the encoder
    ret = avcodec_send_frame(c, tmp_frame);
    if(ret < 0)
    {
      fprintf(stderr, "Error sending a frame to the encoder: %s\n", av_err2str(ret));
      exit(1);
    }

    while(ret >= 0)
    {
      ret = avcodec_receive_packet(c, pkt);
      if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        break;
      else if(ret < 0)
      {
        fprintf(stderr, "Error encoding a frame: %s\n", av_err2str(ret));
        exit(1);
      }

      /* rescale output packet timestamp values from codec to stream timebase */
      av_packet_rescale_ts(pkt, c->time_base, st->time_base);
      pkt->stream_index = st->index;
      pkt->flags |= AV_PKT_FLAG_KEY;

      /* Write the compressed frame to the media file. */
      // log_packet(fmt_ctx, pkt);
      ret = av_interleaved_write_frame(fmt_ctx, pkt);
      /* pkt is now blank (av_interleaved_write_frame() takes ownership of
         * its contents and resets pkt), so that no unreferencing is necessary.
         * This would be different if one used av_write_frame(). */
      if(ret < 0)
      {
        fprintf(stderr, "Error while writing output packet: %s\n", av_err2str(ret));
        exit(1);
      }
    }

    return ret == AVERROR_EOF ? 1 : 0;
  }

  int write_video_frame(AVFormatContext* oc, AVFrame* frame)
  {
    return write_frame(oc, enc, st, frame, tmp_pkt);
  }
};
}
