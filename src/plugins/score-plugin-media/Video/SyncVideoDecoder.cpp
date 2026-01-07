#include "SyncVideoDecoder.hpp"

#include <Media/Libav.hpp>
#include <Video/GpuFormats.hpp>

#include <score/tools/Debug.hpp>

#include <ossia/detail/flicks.hpp>
#include <ossia/detail/libav.hpp>

#include <QDebug>

#if 1 || SCORE_HAS_LIBAV

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}

namespace Video
{

SyncVideoDecoder::SyncVideoDecoder(DecoderConfiguration conf) noexcept
{
  m_conf = std::move(conf);
}

SyncVideoDecoder::~SyncVideoDecoder() noexcept
{
  close_file();
}

bool SyncVideoDecoder::open(const std::string& inputFile) noexcept
{
  close_file();

  m_inputFile = inputFile;

  if(avformat_open_input(&m_formatContext, inputFile.c_str(), nullptr, nullptr) != 0)
  {
    close_file();
    return false;
  }

  if(avformat_find_stream_info(m_formatContext, nullptr) < 0)
  {
    close_file();
    return false;
  }

  if(!open_stream())
  {
    close_file();
    return false;
  }

  int64_t secs = m_formatContext->duration / AV_TIME_BASE;
  int64_t us = m_formatContext->duration % AV_TIME_BASE;

  m_duration = secs * ossia::flicks_per_second<int64_t>;
  m_duration += us * ossia::flicks_per_millisecond<int64_t> / 1000;

  return true;
}

int64_t SyncVideoDecoder::duration() const noexcept
{
  return m_duration;
}

void SyncVideoDecoder::seek(int64_t flicks)
{
  //seek_impl(flicks);
}

bool SyncVideoDecoder::is_frame_at_time(AVFrame* frame, int64_t target_flicks) const noexcept
{
  if(!frame)
    return false;

  double frame_flicks = flicks_per_dts * frame->pts;
  double fps_val = fps > 0. ? fps : 24.;
  double flicks_per_frame = ossia::flicks_per_second<double> / fps_val;

  // Consider frame matching if within 1 frame of target
  double drift = std::abs(target_flicks - frame_flicks) / flicks_per_frame;
  return drift <= 1.0;
}

bool SyncVideoDecoder::needs_seek(int64_t target_flicks) const noexcept
{
  if(m_lastFrameFlicks < 0)
    return true;

  double fps_val = fps > 0. ? fps : 24.;
  double flicks_per_frame = ossia::flicks_per_second<double> / fps_val;

  // Need to seek if we're more than a few frames away
  double drift = (target_flicks - m_lastFrameFlicks) / flicks_per_frame;

  // Seek if we need to go backward, or if we're way ahead (more than 10 frames)
  return drift < -0.5 || drift > 10.0;
}

AVFrame* SyncVideoDecoder::decode_frame_at(int64_t target_flicks) noexcept
{
  // Check if we already have a suitable frame cached
  if(m_lastFrame && is_frame_at_time(m_lastFrame, target_flicks))
  {
    return m_lastFrame;
  }

  // Check if we need to seek
  if(needs_seek(target_flicks))
  {
    seek_impl(target_flicks);
  }

  // Decode frames until we reach the target time
  double fps_val = fps > 0. ? fps : 24.;
  double flicks_per_frame = ossia::flicks_per_second<double> / fps_val;

  int max_iterations = 100; // Safety limit
  while(max_iterations-- > 0)
  {
    AVFrame* frame = read_frame_impl();
    if(!frame)
    {
      // End of file or error - return last frame if we have one
      return m_lastFrame;
    }

    // Release old frame if different
    if(m_lastFrame && m_lastFrame != frame)
    {
      m_frames.release(m_lastFrame);
    }

    m_lastFrame = frame;
    m_lastFrameFlicks = flicks_per_dts * frame->pts;

    // Check if this frame is at or past our target
    double drift = (target_flicks - m_lastFrameFlicks) / flicks_per_frame;

    if(drift <= 1.0)
    {
      // We've reached or passed the target
      return frame;
    }
  }

  // Safety: return whatever we have
  return m_lastFrame;
}

AVFrame* SyncVideoDecoder::dequeue_frame() noexcept
{
  // For compatibility - in sync mode, just return the cached frame
  // Real sync usage should call decode_frame_at() instead
  return m_lastFrame;
}

void SyncVideoDecoder::release_frame(AVFrame* frame) noexcept
{
  // In sync mode, we manage frame lifecycle ourselves
  // Only release if it's not our cached frame
  if(frame && frame != m_lastFrame)
  {
    m_frames.release(frame);
  }
}

bool SyncVideoDecoder::seek_impl(int64_t flicks) noexcept
{
  if(!m_avstream)
    return false;

  if(m_avstream->index >= int(m_formatContext->nb_streams))
    return false;

  // Release cached frame before seeking
  if(m_lastFrame)
  {
    m_frames.release(m_lastFrame);
    m_lastFrame = nullptr;
    m_lastFrameFlicks = -1;
  }

  if(!ossia::seek_to_flick(m_formatContext, m_codecContext, m_avstream, flicks))
  {
    qDebug() << "SyncVideoDecoder: Failed to seek to" << flicks;
    return false;
  }

  m_finished = false;
  m_currentPosition = flicks;

  // Decode one frame to prime the decoder after seeking
  ReadFrame r;
  do
  {
    if(r.frame)
    {
      SCORE_LIBAV_FRAME_DEALLOC_CHECK(r.frame);
      av_frame_free(&r.frame);
    }

    auto pkt = av_packet_alloc();
    r = read_one_frame(*pkt);
    av_packet_unref(pkt);
    av_packet_free(&pkt);
  } while(r.error == AVERROR(EAGAIN));

  if(r.frame)
  {
    m_lastFrame = r.frame;
    m_lastFrameFlicks = flicks_per_dts * r.frame->pts;
  }

  return true;
}

AVFrame* SyncVideoDecoder::read_frame_impl() noexcept
{
  ReadFrame res;

  if(m_avstream)
  {
    auto packet = av_packet_alloc();

    do
    {
      av_packet_unref(packet);
      res = read_one_frame(*packet);

      if(res.error == AVERROR_EOF)
      {
        m_finished = true;
        av_packet_unref(packet);
        av_packet_free(&packet);
        return res.frame;
      }
    } while(res.error == AVERROR(EAGAIN));

    av_packet_unref(packet);
    av_packet_free(&packet);
  }
  return res.frame;
}

bool SyncVideoDecoder::open_stream() noexcept
{
  bool res = false;

  if(!m_formatContext)
    return res;

  int stream = -1;

  for(unsigned int i = 0; i < m_formatContext->nb_streams; i++)
  {
    if(m_formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
    {
      if(stream == -1)
      {
        stream = i;
        continue;
      }
    }
    m_formatContext->streams[i]->discard = AVDISCARD_ALL;
  }

  if(stream != -1)
  {
    m_avstream = m_formatContext->streams[stream];
    const AVRational tb = m_avstream->time_base;
    dts_per_flicks = (tb.den / (tb.num * ossia::flicks_per_second<double>));
    flicks_per_dts = (tb.num * ossia::flicks_per_second<double>) / tb.den;

    auto codecPar = m_avstream->codecpar;
    if((m_codec = avcodec_find_decoder(codecPar->codec_id)))
    {
      if(codecPar->width <= 0 || codecPar->height <= 0)
      {
        qDebug() << "SyncVideoDecoder: invalid video: width or height is 0";
        res = false;
      }
      else
      {
        color_range = m_avstream->codecpar->color_range;
        color_primaries = m_avstream->codecpar->color_primaries;
        color_trc = m_avstream->codecpar->color_trc;
        color_space = m_avstream->codecpar->color_space;
        chroma_location = m_avstream->codecpar->chroma_location;

        if(color_space == AVCOL_SPC_UNSPECIFIED)
        {
          if(codecPar->height < 625)
            color_space = AVCOL_SPC_SMPTE170M;
          else if(codecPar->height < 720)
            color_space = AVCOL_SPC_BT470BG;
          else if(codecPar->height < 2160)
            color_space = AVCOL_SPC_BT709;
          else
            color_space = AVCOL_SPC_BT2020_CL;
        }

        if(m_avstream->codecpar->codec_id == AV_CODEC_ID_HAP)
        {
          memcpy(&pixel_format, &m_avstream->codecpar->codec_tag, 4);
          width = codecPar->width;
          height = codecPar->height;
          fps = av_q2d(m_avstream->avg_frame_rate);

          m_conf.useAVCodec = false;
          m_codecContext = nullptr;
          m_codec = nullptr;
          res = true;
        }
        else
        {
          pixel_format = (AVPixelFormat)codecPar->format;
          width = codecPar->width;
          height = codecPar->height;
          fps = av_q2d(m_avstream->avg_frame_rate);

          res = open_codec_context(*this, m_avstream, [this](AVCodecContext& ctx) {
            ctx.framerate
                = av_guess_frame_rate(m_formatContext, (AVStream*)m_avstream, NULL);
            m_codecContext->pkt_timebase = m_avstream->time_base;
          });

          if(m_codecContext)
          {
            auto tb = m_codecContext->pkt_timebase;
            dts_per_flicks = (tb.den / (tb.num * ossia::flicks_per_second<double>));
            flicks_per_dts = (tb.num * ossia::flicks_per_second<double>) / tb.den;
          }
        }
      }
    }
  }

  if(!res)
  {
    close_video();
  }
  return res;
}

void SyncVideoDecoder::close_file() noexcept
{
  // Release cached frame
  if(m_lastFrame)
  {
    m_frames.release(m_lastFrame);
    m_lastFrame = nullptr;
    m_lastFrameFlicks = -1;
  }

  // Drain the frame queue
  m_frames.drain();

  // Clear the stream
  close_video();

  // Clear the fmt context
  if(m_formatContext)
  {
    avio_flush(m_formatContext->pb);
    avformat_flush(m_formatContext);
    avformat_close_input(&m_formatContext);
    avformat_free_context(m_formatContext);
    m_formatContext = nullptr;
  }
}

void SyncVideoDecoder::close_video() noexcept
{
  if(m_codecContext)
  {
    avcodec_flush_buffers(m_codecContext);
#if defined(__APPLE__)
#if FF_API_VT_HWACCEL_CONTEXT
    if(m_codecContext->hwaccel_context)
      av_videotoolbox_default_free(m_codecContext);
#endif
#endif
    avcodec_free_context(&m_codecContext);

    m_codecContext = nullptr;
    m_codec = nullptr;
  }

  m_rescale.close();

  m_avstream = nullptr;
}

}
#endif
