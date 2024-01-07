#include "LibavEncoder.hpp"

#include "LibavOutputStream.hpp"

#include <QDebug>
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

LibavEncoder::~LibavEncoder()
{
  av_dict_free(&opt);
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

int LibavEncoder::start()
{
  const char* filename = "/tmp/tata.mp4";
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

  // For each parameter:
  // Add a streamr
  {
    StreamOptions opts;
    opts.codec = "libx264rgb";
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
  return 0;
}

int LibavEncoder::add_frame(
    const unsigned char* data, AVPixelFormat fmt, int width, int height)
{
  auto& stream = streams[0];
  auto next_frame = stream.get_video_frame();

  auto out = next_frame->data[0];
  for(int y = 0; y < height; y++)
  {
    for(int x = 0; x < width; x++)
    {
      out[0] = data[0];
      out[1] = data[1];
      out[2] = data[2];

      out += 3;
      data += 4;
    }
  }
  return streams[0].write_video_frame(m_formatContext, next_frame);
}

int LibavEncoder::stop()
{
  if(!m_formatContext)
    return 0;

  const AVOutputFormat* fmt = m_formatContext->oformat;
  av_write_trailer(m_formatContext);

  for(auto& stream : streams)
    stream.close(m_formatContext);
  streams.clear();

  if(!(fmt->flags & AVFMT_NOFILE))
    avio_closep(&m_formatContext->pb);

  avformat_free_context(m_formatContext);
  m_formatContext = nullptr;

  return 0;
}

}
#endif
