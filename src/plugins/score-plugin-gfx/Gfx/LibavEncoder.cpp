#include "LibavEncoder.hpp"

extern "C" {
#include <libswresample/swresample.h>
}

#if SCORE_HAS_LIBAV
namespace Gfx
{
LibavEncoder::LibavEncoder()
{
  av_dict_set(&opt, "fflags", "nobuffer", 0);
  av_dict_set(&opt, "flags", "low_delay", 0);
}

void LibavEncoder::enumerate()
{
#if 0
    // enumerate all codecs and put into list
    std::vector<const AVCodec*> encoderList;
    AVCodec* codec = nullptr;
    while(codec = av_codec_next(codec))
    {
      // try to get an encoder from the system
      auto encoder = avcodec_find_encoder(codec->id);
      if(encoder)
      {
        encoderList.push_back(encoder);
      }
    }
    // enumerate all containers
    AVOutputFormat* outputFormat = nullptr;
    while(outputFormat = av_oformat_next(outputFormat))
    {
      for(auto codec : encoderList)
      {
        // only add the codec if it can be used with this container
        if(avformat_query_codec(outputFormat, codec->id, FF_COMPLIANCE_STRICT) == 1)
        {
          // add codec for container
        }
      }
    }
#endif
}

void LibavEncoder::encode(
    AVCodecContext* enc_ctx, AVFrame* frame, AVPacket* pkt, FILE* outfile)
{
  int ret;

  /* send the frame to the encoder */
  if(frame)
    printf("Send frame %3" PRId64 "\n", frame->pts);

  ret = avcodec_send_frame(enc_ctx, frame);
  if(ret < 0)
  {
    fprintf(stderr, "Error sending a frame for encoding\n");
    exit(1);
  }

  while(ret >= 0)
  {
    ret = avcodec_receive_packet(enc_ctx, pkt);
    if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
      return;
    else if(ret < 0)
    {
      fprintf(stderr, "Error during encoding\n");
      exit(1);
    }

    printf("Write packet %3" PRId64 " (size=%5d)\n", pkt->pts, pkt->size);
    fwrite(pkt->data, 1, pkt->size, outfile);
    av_packet_unref(pkt);
  }
}

struct StreamOptions
{
  std::string name;
  std::string codec;
  ossia::flat_map<std::string, std::string> options;
};

struct OutputStream
{
  const AVCodec* codec;
  AVStream* st;
  AVCodecContext* enc;

  /* pts of the next frame that will be generated */
  int64_t next_pts;
  int samples_count;

  AVFrame* frame;
  AVFrame* tmp_frame;

  AVPacket* tmp_pkt;

  struct SwsContext* sws_ctx;
  struct SwrContext* swr_ctx;

  OutputStream(AVFormatContext* oc, const StreamOptions& opts)
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
        constexpr AVChannelLayout layout = AV_CHANNEL_LAYOUT_STEREO;
        av_channel_layout_copy(&c->ch_layout, &layout);
        this->st->time_base = (AVRational){1, c->sample_rate};
        break;
      }

      case AVMEDIA_TYPE_VIDEO: {
        c->codec_id = codec->id;

        c->bit_rate = 400000;
        /* Resolution must be a multiple of two. */
        c->width = 352;
        c->height = 288;
        /* timebase: This is the fundamental unit of time (in seconds) in terms
         * of which frame timestamps are represented. For fixed-fps content,
         * timebase should be 1/framerate and timestamp increments should be
         * identical to 1. */
        this->st->time_base = (AVRational){1, 30};
        c->time_base = this->st->time_base;
        c->framerate = AVRational{30, 1};

        //c->gop_size = 12; /* emit one intra frame every twelve frames at most */

        // ignored if frame->pict_type is AV_PICTURE_TYPE_I
        c->gop_size = 0;
        c->max_b_frames = 0;
        c->pix_fmt = AV_PIX_FMT_YUV420P;
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
      c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
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

  void open_video(AVFormatContext* oc, const AVCodec* codec, AVDictionary* opt_arg)
  {
    int ret;
    AVCodecContext* c = this->enc;
    AVDictionary* opt = NULL;

    av_dict_copy(&opt, opt_arg, 0);

    /* open the codec */
    ret = avcodec_open2(c, codec, &opt);
    av_dict_free(&opt);
    if(ret < 0)
    {
      fprintf(stderr, "Could not open video codec: %s\n", av_err2str(ret));
      exit(1);
    }

    /* allocate and init a re-usable frame */
    // this->frame = alloc_frame(c->pix_fmt, c->width, c->height);
    // if(!this->frame)
    // {
    //   fprintf(stderr, "Could not allocate video frame\n");
    //   exit(1);
    // }

    /* If the output format is not YUV420P, then a temporary YUV420P
     * picture is needed too. It is then converted to the required
     * output format. */
    // If conversion is needed :
    // this->tmp_frame = NULL;
    // if(c->pix_fmt != AV_PIX_FMT_YUV420P)
    // {
    //   this->tmp_frame = alloc_frame(AV_PIX_FMT_YUV420P, c->width, c->height);
    //   if(!this->tmp_frame)
    //   {
    //     fprintf(stderr, "Could not allocate temporary video frame\n");
    //     exit(1);
    //   }
    // }

    /* copy the stream parameters to the muxer */
    ret = avcodec_parameters_from_context(this->st->codecpar, c);
    if(ret < 0)
    {
      fprintf(stderr, "Could not copy the stream parameters\n");
      exit(1);
    }
  }

  void open(AVFormatContext* oc, const AVCodec* codec, AVDictionary* opt_arg)
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
      open_video(oc, codec, opt_arg);
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
    AVCodecContext* c = this->enc;

    /* check if we want to generate more frames */
    // if (av_compare_ts(ost->next_pts, c->time_base,
    //                  STREAM_DURATION, (AVRational){ 1, 1 }) > 0)
    //   return NULL;

    /* when we pass a frame to the encoder, it may keep a reference to it
     * internally; make sure we do not overwrite it here */
    if(av_frame_make_writable(this->frame) < 0)
      exit(1);

    // if(c->pix_fmt != AV_PIX_FMT_YUV420P)
    // {
    //   if(!this->sws_ctx)
    //   {
    //     this->sws_ctx = sws_getContext(
    //         c->width, c->height, AV_PIX_FMT_YUV420P, c->width, c->height, c->pix_fmt,
    //         SCALE_FLAGS, NULL, NULL, NULL);
    //     if(!this->sws_ctx)
    //     {
    //       fprintf(stderr, "Could not initialize the conversion context\n");
    //       exit(1);
    //     }
    //   }
    //   fill_yuv_image(this->tmp_frame, this->next_pts, c->width, c->height);
    //   sws_scale(
    //       this->sws_ctx, (const uint8_t* const*)this->tmp_frame->data,
    //       this->tmp_frame->linesize, 0, c->height, this->frame->data,
    //       this->frame->linesize);
    // }
    // else
    // {
    //   fill_yuv_image(this->frame, this->next_pts, c->width, c->height);
    // }

    this->frame->pts = this->next_pts++;

    return this->frame;
  }

  static int write_frame(
      AVFormatContext* fmt_ctx, AVCodecContext* c, AVStream* st, AVFrame* frame,
      AVPacket* pkt)
  {
    int ret;

    // send the frame to the encoder
    ret = avcodec_send_frame(c, frame);
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

  int write_video_frame(AVFormatContext* oc)
  {
    return write_frame(oc, enc, st, get_video_frame(), tmp_pkt);
  }
};
int LibavEncoder::test2()
{
  const char* filename = "/tmp/tata.mkv";
  int ret = avformat_alloc_output_context2(&m_formatContext, NULL, NULL, filename);

  if(ret < 0 || !m_formatContext)
  {
    fprintf(stderr, "Could not create format for '%s': %s\n", filename, av_err2str(ret));
    return 1;
  }

  const AVOutputFormat* fmt = m_formatContext->oformat;
  SCORE_ASSERT(fmt);

  qDebug() << fmt->name << fmt->long_name << fmt->audio_codec
           << fmt->video_codec; // matroska / Matroska
  // fmt->audio_codec: default audio codec for mkv
  // fmt->video_codec: default video codec for mkv
  auto default_audio_encoder = avcodec_find_encoder(fmt->audio_codec);
  auto default_video_encoder = avcodec_find_decoder(fmt->video_codec);
  qDebug() << "Codec:" << default_audio_encoder->name << default_video_encoder->name;

  /* Add the audio and video streams using the default format codecs
     * and initialize the codecs. */
  if(fmt->video_codec != AV_CODEC_ID_NONE)
  {
    // add_stream(&video_st, oc, &video_codec, fmt->video_codec);
    // have_video = 1;
    // encode_video = 1;
  }
  if(fmt->audio_codec != AV_CODEC_ID_NONE)
  {
    // add_stream(&audio_st, oc, &audio_codec, fmt->audio_codec);
    // have_audio = 1;
    // encode_audio = 1;
  }

  // For each parameter:

  std::vector<OutputStream> streams;
  // For all streams:
  // Add them
  {
    StreamOptions opts;
    opts.codec = "hevc_nvenc";
    streams.emplace_back(m_formatContext, opts);
  }

  // For all streams:
  // Open them
  for(auto& stream : streams)
  {
    stream.open(m_formatContext, stream.codec, opt);
  }

  // Dump all streams
  {
    int k = 0;
    for(auto& stream : streams)
    {
      av_dump_format(m_formatContext, k++, filename, true);
    }
  }

  // If it's a file fopen it
  if(!(fmt->flags & AVFMT_NOFILE))
  {
    ret = avio_open(&m_formatContext->pb, filename, AVIO_FLAG_WRITE);
    if(ret < 0)
    {
      fprintf(stderr, "Could not open '%s': %s\n", filename, av_err2str(ret));
      return 1;
    }
  }

  // Init stream header
  ret = avformat_write_header(m_formatContext, &opt);
  if(ret < 0)
  {
    fprintf(stderr, "Error occurred when opening output file: %s\n", av_err2str(ret));
    return 1;
  }

  //////////////////////:
  // Encode streams here
  //////////////////////:

  // Close
  {
    av_write_trailer(m_formatContext);

    // For each stream
    for(auto& stream : streams)
      stream.close(m_formatContext);

    // if it's a file, fclose it
    if(!(fmt->flags & AVFMT_NOFILE))
      avio_closep(&m_formatContext->pb);

    // We're done
    avformat_free_context(m_formatContext);
  }
  return 0;
}

void LibavEncoder::test()
{
  test2();
  return;
#if 0
  {
    const char *filename, *codec_name;

    int i, ret, x, y;
    FILE* f;
    uint8_t endcode[] = {0, 0, 1, 0xb7};

    filename = "/tmp/toto.mkv";
    codec_name = "hevc_nvenc";

    /* find the mpeg1video encoder */
    m_codec = avcodec_find_encoder_by_name(codec_name);
    if(!m_codec)
    {
      fprintf(stderr, "Codec '%s' not found\n", codec_name);
      exit(1);
    }

    m_codecContext = avcodec_alloc_context3(m_codec);
    if(!m_codecContext)
    {
      fprintf(stderr, "Could not allocate video codec context\n");
      exit(1);
    }

    pkt = av_packet_alloc();
    if(!pkt)
      exit(1);

    /* put sample parameters */
    m_codecContext->bit_rate = 400000;
    /* resolution must be a multiple of two */
    m_codecContext->width = 352;
    m_codecContext->height = 288;
    /* frames per second */
    m_codecContext->time_base = (AVRational){1, 25};
    m_codecContext->framerate = (AVRational){25, 1};

    /* emit one intra frame every ten frames
     * check frame pict_type before passing frame
     * to encoder, if frame->pict_type is AV_PICTURE_TYPE_I
     * then gop_size is ignored and the output of encoder
     * will always be I frame irrespective to gop_size
     */
    m_codecContext->gop_size = 0;
    m_codecContext->max_b_frames = 0;
    m_codecContext->pix_fmt = AV_PIX_FMT_YUV420P;

    if(m_codec->id == AV_CODEC_ID_H264)
      av_opt_set(m_codecContext->priv_data, "preset", "slow", 0);

    /* open it */
    ret = avcodec_open2(m_codecContext, m_codec, NULL);
    if(ret < 0)
    {
      fprintf(stderr, "Could not open codec: %s\n", av_err2str(ret));
      exit(1);
    }

    f = fopen(filename, "wb");
    if(!f)
    {
      fprintf(stderr, "Could not open %s\n", filename);
      exit(1);
    }

    frame = av_frame_alloc();
    if(!frame)
    {
      fprintf(stderr, "Could not allocate video frame\n");
      exit(1);
    }
    frame->format = m_codecContext->pix_fmt;
    frame->width = m_codecContext->width;
    frame->height = m_codecContext->height;

    ret = av_frame_get_buffer(frame, 0);
    if(ret < 0)
    {
      fprintf(stderr, "Could not allocate the video frame data\n");
      exit(1);
    }

    /* encode 1 second of video */
    for(i = 0; i < 25; i++)
    {
      fflush(stdout);

      /* Make sure the frame data is writable.
           On the first round, the frame is fresh from av_frame_get_buffer()
           and therefore we know it is writable.
           But on the next rounds, encode() will have called
           avcodec_send_frame(), and the codec may have kept a reference to
           the frame in its internal structures, that makes the frame
           unwritable.
           av_frame_make_writable() checks that and allocates a new buffer
           for the frame only if necessary.
         */
      ret = av_frame_make_writable(frame);
      if(ret < 0)
        exit(1);

      /* Prepare a dummy image.
           In real code, this is where you would have your own logic for
           filling the frame. FFmpeg does not care what you put in the
           frame.
         */

      // m_codecContext->height
      // m_codecContext->width
      // Y
      frame->data[0][0] = rand();
      // Cb Cr
      frame->data[1]; // h, w, are divided by 2
      frame->data[2];

      frame->pts = i;

      /* encode the image */
      encode(m_codecContext, frame, pkt, f);
    }

    /* flush the encoder */
    encode(m_codecContext, NULL, pkt, f);

    /* Add sequence end code to have a real MPEG file.
       It makes only sense because this tiny examples writes packets
       directly. This is called "elementary stream" and only works for some
       codecs. To create a valid file, you usually need to write packets
       into a proper file format or protocol; see mux.c.
     */
    if(m_codec->id == AV_CODEC_ID_MPEG1VIDEO || m_codec->id == AV_CODEC_ID_MPEG2VIDEO)
      fwrite(endcode, 1, sizeof(endcode), f);
    fclose(f);

    avcodec_free_context(&m_codecContext);
    av_frame_free(&frame);
    av_packet_free(&pkt);
  }
#endif
}

}
#endif
