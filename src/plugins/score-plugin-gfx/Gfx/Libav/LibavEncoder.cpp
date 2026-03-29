#include "LibavEncoder.hpp"

#include "LibavOutputStream.hpp"
extern "C" {
#include <libswresample/swresample.h>
}

#include <QDebug>
#include <mutex>

#if SCORE_HAS_LIBAV
namespace Gfx
{
LibavEncoder::LibavEncoder(const LibavOutputSettings& set)
    : m_set{set}
{
}

LibavEncoder::~LibavEncoder() { }

int LibavEncoder::start()
{
  if(m_set.path.isEmpty())
    return 1;

  // Build options dictionary from settings
  AVDictionary* opt = nullptr;
  for(const auto& [k, v] : m_set.options)
    av_dict_set(&opt, k.toStdString().c_str(), v.toStdString().c_str(), 0);

  const auto muxer = m_set.muxer.toStdString();
  const auto filename = m_set.path.toStdString();
  int ret = avformat_alloc_output_context2(
      &m_formatContext, nullptr, muxer.c_str(), filename.c_str());

  if(ret < 0 || !m_formatContext)
  {
    qDebug() << "Could not create format for: " << muxer.c_str() << filename.c_str()
             << av_to_string(ret);
    m_formatContext = nullptr;
    av_dict_free(&opt);
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
    opts.codec = m_set.video_encoder_short.toStdString();
    streams.emplace_back(m_set, m_formatContext, opts);
    video_stream_index = k;
    k++;
  }
  if(!m_set.audio_encoder_short.isEmpty())
  {
    StreamOptions opts;
    opts.codec = m_set.audio_encoder_short.toStdString();
    streams.emplace_back(m_set, m_formatContext, opts);
    audio_stream_index = k;
    k++;
  }

  // For all streams:
  // Open them. Each codec receives a copy of the options dict — codecs consume
  // the keys they recognize and ignore the rest.
  for(auto& stream : streams)
  {
    stream.open(m_set, m_formatContext, stream.codec, opt);
    if(!stream.m_valid)
    {
      qDebug() << "Failed to open stream";
      for(auto& s : streams)
        s.close(m_formatContext);
      streams.clear();
      audio_stream_index = -1;
      video_stream_index = -1;
      avformat_free_context(m_formatContext);
      m_formatContext = nullptr;
      av_dict_free(&opt);
      return 1;
    }
  }

  av_dump_format(m_formatContext, 0, filename.c_str(), 1);

  // If it's a file fopen it
  if(!(fmt->flags & AVFMT_NOFILE))
  {
    ret = avio_open(&m_formatContext->pb, filename.c_str(), AVIO_FLAG_WRITE);
    if(ret < 0)
    {
      qDebug() << "Could not open" << filename.c_str() << ":" << av_to_string(ret);
      for(auto& s : streams)
        s.close(m_formatContext);
      streams.clear();
      audio_stream_index = -1;
      video_stream_index = -1;
      avformat_free_context(m_formatContext);
      m_formatContext = nullptr;
      av_dict_free(&opt);
      return 1;
    }
  }

  // Init stream header — pass remaining options (muxer-level) to avformat
  ret = avformat_write_header(m_formatContext, &opt);
  av_dict_free(&opt);
  if(ret < 0)
  {
    qDebug() << "Error occurred when opening output file:" << av_to_string(ret);
    for(auto& s : streams)
      s.close(m_formatContext);
    streams.clear();
    audio_stream_index = -1;
    video_stream_index = -1;
    if(!(fmt->flags & AVFMT_NOFILE))
      avio_closep(&m_formatContext->pb);
    avformat_free_context(m_formatContext);
    m_formatContext = nullptr;
    return 1;
  }
  return 0;
}

int LibavEncoder::add_frame(std::span<ossia::float_vector> vec)
{
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 24, 100)
  std::lock_guard lock{m_muxMutex};

  if(!m_formatContext)
    return 1;
  if(audio_stream_index < 0)
    return 1;

  const int channels = vec.size();
  if(channels == 0) // Write silence?
    return 1;

  auto& stream = streams[audio_stream_index];
  AVFrame* next_frame = stream.get_audio_frame();
  if(!next_frame)
    return 1;

  next_frame->sample_rate = stream.enc->sample_rate;
  next_frame->format = stream.enc->sample_fmt;
  next_frame->nb_samples = stream.enc->frame_size;
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

    // Convert float input to double for r8b resampler, then resample
    stream.resample_in_buf.resize(channels);
    stream.resample_out_buf.resize(channels);
    for(int c = 0; c < channels; c++)
    {
      stream.resample_in_buf[c].assign(vec[c].begin(), vec[c].end());
      double* ret{};
      int res = stream.resamplers[c]->process(
          stream.resample_in_buf[c].data(), vec[c].size(), ret);
      stream.resample_out_buf[c].assign(ret, ret + res);
    }

    stream.encoder->add_frame(*next_frame, stream.resample_out_buf);
  }

  return stream.write_audio_frame(m_formatContext, next_frame);
#endif
  return 1;
}

int LibavEncoder::add_frame(
    const unsigned char* data, AVPixelFormat fmt, int width, int height)
{
  std::lock_guard lock{m_muxMutex};

  if(!m_formatContext)
    return 1;
  if(video_stream_index < 0)
    return 1;

  auto& stream = streams[video_stream_index];
  auto next_frame = stream.get_video_frame();
  if(!next_frame)
    return 1;

  // Copy pixel data into the frame's own buffer (don't alias external memory)
  if(next_frame->data[0] && next_frame->linesize[0] > 0)
  {
    int dst_stride = next_frame->linesize[0];
    int src_stride = width * 4;
    if(dst_stride == src_stride)
    {
      memcpy(next_frame->data[0], data, width * height * 4);
    }
    else
    {
      for(int y = 0; y < height; y++)
        memcpy(next_frame->data[0] + y * dst_stride, data + y * src_stride, src_stride);
    }
  }

  return stream.write_video_frame(m_formatContext, next_frame);
}

int LibavEncoder::add_frame_converted(
    const unsigned char* const planes[], const int strides[], int planeCount,
    int width, int height)
{
  std::lock_guard lock{m_muxMutex};

  if(!m_formatContext)
    return 1;
  if(video_stream_index < 0)
    return 1;

  auto& stream = streams[video_stream_index];
  if(!stream.m_valid || !stream.tmp_frame)
    return 1;

  // Get the next PTS
  auto* frame = stream.tmp_frame;
  frame->pts = stream.next_pts++;

  // Copy pre-converted plane data directly into the encoder's tmp_frame.
  // tmp_frame is already allocated in the target pixel format.
  for(int i = 0; i < planeCount && i < AV_NUM_DATA_POINTERS; i++)
  {
    if(!frame->data[i] || !planes[i])
      continue;

    int dst_stride = frame->linesize[i];
    int src_stride = strides[i];

    // Determine plane height (chroma planes are half height for 4:2:0)
    int plane_height = height;
    if(i > 0)
    {
      // For YUV420 formats, chroma planes are half height
      const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get((AVPixelFormat)frame->format);
      if(desc)
        plane_height = -((-height) >> desc->log2_chroma_h);
    }

    if(dst_stride == src_stride)
    {
      memcpy(frame->data[i], planes[i], dst_stride * plane_height);
    }
    else
    {
      int copy_bytes = std::min(dst_stride, src_stride);
      for(int y = 0; y < plane_height; y++)
        memcpy(
            frame->data[i] + y * dst_stride, planes[i] + y * src_stride,
            copy_bytes);
    }
  }

  // Write directly to the encoder — skip sws_scale entirely
  return stream.write_video_frame_direct(m_formatContext, frame);
}

int LibavEncoder::stop()
{
  std::lock_guard lock{m_muxMutex};
  return stop_impl();
}

int LibavEncoder::stop_impl()
{
  if(!m_formatContext)
    return 0;

  const AVOutputFormat* fmt = m_formatContext->oformat;

  // Flush all encoders by sending NULL frame
  for(auto& stream : streams)
  {
    if(!stream.m_valid || !stream.enc)
      continue;
    avcodec_send_frame(stream.enc, nullptr);
    while(true)
    {
      int ret = avcodec_receive_packet(stream.enc, stream.tmp_pkt);
      if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        break;
      if(ret < 0)
        break;
      av_packet_rescale_ts(
          stream.tmp_pkt, stream.enc->time_base, stream.st->time_base);
      stream.tmp_pkt->stream_index = stream.st->index;
      av_interleaved_write_frame(m_formatContext, stream.tmp_pkt);
    }
  }

  av_write_trailer(m_formatContext);

  for(auto& stream : streams)
    stream.close(m_formatContext);
  streams.clear();
  audio_stream_index = -1;
  video_stream_index = -1;

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
