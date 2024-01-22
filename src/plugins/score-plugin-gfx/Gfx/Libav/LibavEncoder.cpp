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
  for(const auto& [k, v] : m_set.options)
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

  // For each parameter:
  // Add a stream
  int k = 0;
  if(!m_set.video_encoder_short.isEmpty())
  {
    StreamOptions opts;
    // FIXME need to pass a codec id instead
    opts.codec = m_set.video_encoder_short.toStdString();
    streams.emplace_back(m_set, m_formatContext, opts);
    video_stream_index = k;
    k++;
  }
  if(!m_set.audio_encoder_short.isEmpty())
  {
    StreamOptions opts;
    // FIXME need to pass a codec id instead
    opts.codec = m_set.audio_encoder_short.toStdString();
    streams.emplace_back(m_set, m_formatContext, opts);
    audio_stream_index = k;
    k++;
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

int LibavEncoder::add_frame(tcb::span<ossia::float_vector> vec)
{
  if(!m_formatContext)
    return 1;

  const int channels = vec.size();
  if(channels == 0) // Write silence?
    return 1;

  auto& stream = streams[audio_stream_index];
  AVFrame* next_frame = stream.get_audio_frame();

  next_frame->sample_rate = SAMPLE_RATE_TEST;
  next_frame->format = stream.enc->sample_fmt;
  next_frame->nb_samples = vec[0].size();
  next_frame->ch_layout.nb_channels = channels;
  next_frame->ch_layout.order = AV_CHANNEL_ORDER_UNSPEC;

  // Write the data
  if(stream.resamplers.empty())
  {
    // Encode directly
    stream.encoder->add_frame(*next_frame, vec);
  }
  else
  {
    if(vec.size() != stream.resamplers.size())
    {
      qDebug() << "Error: invalid channel count for expected resampling.";
      return 1;
    }

    std::vector<std::vector<double>> resample_in(channels);
    std::vector<ossia::float_vector> resample_outf(channels);
    for(int c = 0; c < channels; c++)
    {
      double* ret{};
      int res = stream.resamplers[c]->process(resample_in[c].data(), vec[c].size(), ret);
      resample_outf[c].assign(ret, ret + res);
    }

    stream.encoder->add_frame(*next_frame, resample_outf);
  }
  return stream.write_audio_frame(m_formatContext, next_frame);
}

int LibavEncoder::add_frame(
    const unsigned char* data, AVPixelFormat fmt, int width, int height)
{
  if(!m_formatContext)
    return 1;

  auto& stream = streams[video_stream_index];
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
  return streams[video_stream_index].write_video_frame(m_formatContext, next_frame);
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
