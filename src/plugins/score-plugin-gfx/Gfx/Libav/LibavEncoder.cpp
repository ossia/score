#include "LibavEncoder.hpp"

#include "LibavOutputStream.hpp"

#include <QDebug>
extern "C" {
#include <libswresample/swresample.h>
}

#if SCORE_HAS_LIBAV
namespace Gfx
{
LibavEncoder::LibavEncoder(const LibavOutputSettings& set)
    : m_set{set}
{
  for(const auto& [k, v] : set.options)
  {
    av_dict_set(&opt, k.toStdString().c_str(), v.toStdString().c_str(), 0);
  }
  if(!opt)
    av_dict_set(&opt, "", "", 0);
}

LibavEncoder::~LibavEncoder()
{
  av_dict_free(&opt);
}

int LibavEncoder::start()
{
  const auto muxer = m_set.muxer.toStdString();
  const auto filename = m_set.path.toStdString();
  int ret = avformat_alloc_output_context2(
      &m_formatContext, nullptr, muxer.c_str(), filename.c_str());

  if(ret < 0 || !m_formatContext)
  {
    fprintf(
        stderr, "Could not create format for '%s' - '%s': %s\n", muxer.c_str(),
        filename.c_str(), av_err2str(ret));
    m_formatContext = nullptr;
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
  if(default_audio_encoder)
    qDebug() << "Default Audio Codec:" << default_audio_encoder->name;
  if(default_video_encoder)
    qDebug() << "Default Video Codec:" << default_video_encoder->name;

  // For each parameter:
  // Add a streamr
  {
    StreamOptions opts;
    opts.codec = m_set.video_encoder_short
                     .toStdString(); // FIXME need to pass a codec id instead
    streams.emplace_back(m_set, m_formatContext, opts);
  }

  // For all streams:
  // Open them
  for(auto& stream : streams)
  {
    stream.open(m_set, m_formatContext, stream.codec, opt);
  }

  // Dump all streams
  {
    int k = 0;
    for(auto& stream : streams)
    {
      av_dump_format(m_formatContext, k++, filename.c_str(), true);
    }
  }

  // If it's a file fopen it
  if(!(fmt->flags & AVFMT_NOFILE))
  {
    ret = avio_open(&m_formatContext->pb, filename.c_str(), AVIO_FLAG_WRITE);
    if(ret < 0)
    {
      fprintf(stderr, "Could not open '%s': %s\n", filename.c_str(), av_err2str(ret));
      avformat_free_context(m_formatContext);
      m_formatContext = nullptr;
      return 1;
    }
  }

  // Init stream header
  ret = avformat_write_header(m_formatContext, &opt);
  if(ret < 0)
  {
    fprintf(stderr, "Error occurred when opening output file: %s\n", av_err2str(ret));
    avformat_free_context(m_formatContext);
    m_formatContext = nullptr;
    return 1;
  }
  return 0;
}

int LibavEncoder::add_frame(
    const unsigned char* data, AVPixelFormat fmt, int width, int height)
{
  auto& stream = streams[0];
  auto next_frame = stream.get_video_frame();
  next_frame->format = fmt;
  next_frame->width = width;
  next_frame->height = height;
  next_frame->data[0] = (unsigned char*)data;
  /*
  auto out = next_frame->data[0];
  for(int y = 0; y < height; y++)
  {
    for(int x = 0; x < width; x++)
    {
      out[0] = data[0];
      out[1] = data[1];
      out[2] = data[2];

      out += 4;
      data += 4;
    }
  }
*/
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
  {
    avio_closep(&m_formatContext->pb);
  }
  avformat_free_context(m_formatContext);
  m_formatContext = nullptr;

  return 0;
}

}
#endif
