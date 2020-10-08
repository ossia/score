
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}
#include "VideoDecoder.hpp"

#include <ossia/detail/flicks.hpp>
#include <score/tools/Debug.hpp>

#include <QDebug>
#include <QElapsedTimer>

#include <functional>
namespace Video
{
VideoInterface::~VideoInterface()
{

}


VideoDecoder::VideoDecoder() noexcept { }

VideoDecoder::~VideoDecoder() noexcept
{
  close_file();
}

bool VideoDecoder::load(const std::string& inputFile, double fps_unused) noexcept
{
  close_file();

  if (avformat_open_input(&m_formatContext, inputFile.c_str(), nullptr, nullptr) != 0)
  {
    close_file();
    return false;
  }

  if (avformat_find_stream_info(m_formatContext, nullptr) < 0)
  {
    close_file();
    return false;
  }

  if (!open_stream())
  {
    close_file();
    return false;
  }

  m_running.store(true, std::memory_order_release);
  // TODO use a thread pool
  m_thread = std::thread{[this] { this->buffer_thread(); }};

  int64_t secs = m_formatContext->duration / AV_TIME_BASE;
  int64_t us = m_formatContext->duration % AV_TIME_BASE;

  m_duration = secs * ossia::flicks_per_second<int64_t>;
  m_duration += us * ossia::flicks_per_millisecond<int64_t> / 1000;

  return true;
}


int64_t VideoDecoder::duration() const noexcept
{
  return m_duration;
}

void VideoDecoder::seek(int64_t flicks)
{
  m_seekTo = flicks;
}

AVFrame* VideoDecoder::dequeue_frame() noexcept
{
  AVFrame* f{};
  if (auto to_discard = m_discardUntil.exchange(nullptr))
  {
    while (m_framesToPlayer.try_dequeue(f) && f != to_discard)
    {
      m_releasedFrames.enqueue(f);
    }

    return to_discard;
  }

  m_framesToPlayer.try_dequeue(f);
  m_condVar.notify_one();
  return f;
}

void VideoDecoder::release_frame(AVFrame* frame) noexcept
{
  m_releasedFrames.enqueue(frame);
}

void VideoDecoder::buffer_thread() noexcept
{
  while (m_running.load(std::memory_order_acquire))
  {
    if (int64_t seek = m_seekTo.exchange(-1); seek >= 0)
    {
      seek_impl(seek);
    }
    else
    {
      std::unique_lock lck{m_condMut};
      m_condVar.wait(lck, [&] {
        return m_framesToPlayer.size_approx() < frames_to_buffer / 2
               || !m_running.load(std::memory_order_acquire)
               || (m_seekTo != -1);
      });
      if (!m_running.load(std::memory_order_acquire))
        return;

      if (int64_t seek = m_seekTo.exchange(-1); seek >= 0)
      {
        seek_impl(seek);
      }

      if (m_framesToPlayer.size_approx() < (frames_to_buffer / 2))
      if (auto f = read_frame_impl())
      {
        m_last_dts = f->pkt_dts;
        m_framesToPlayer.enqueue(f);
      }
    }
  }
}

void VideoDecoder::close_file() noexcept
{
  // Stop the running status
  m_running.store(false, std::memory_order_release);
  m_condVar.notify_one();

  if (m_thread.joinable())
    m_thread.join();

  // Clear the stream
  close_video();

  // Clear the fmt context
  if (m_formatContext)
  {
    avformat_close_input(&m_formatContext);
    m_formatContext = nullptr;
  }

  // Remove frames that were in flight
  drain_frames();
}

AVFrame* VideoDecoder::get_new_frame() noexcept
{
  AVFrame* f{};
  if(m_releasedFrames.try_dequeue(f))
    return f;
  return av_frame_alloc();
}

void VideoDecoder::drain_frames() noexcept
{
  AVFrame* frame{};
  while (m_framesToPlayer.try_dequeue(frame))
  {
    av_frame_free(&frame);
  }

  // TODO we must check that this is safe as the queue
  // does not support dequeueing from the same thread as the
  // enqueuing
  while (m_releasedFrames.try_dequeue(frame))
  {
    av_frame_free(&frame);
  }
}

bool VideoDecoder::seek_impl(int64_t flicks) noexcept
{
  if(m_stream >= int(m_formatContext->nb_streams))
    return false;

  const int64_t dts = flicks * dts_per_flicks;

  constexpr int64_t min_dts_delta = 20000;
  if(std::abs(dts - m_last_dts) < min_dts_delta)
    return false;

  // TODO - maybe we should also store the "last dequeued dts" from the
  // decoder side - this way no need to seek if we are in the interval

  const bool seek_forward = dts >= this->m_last_dts;
  if (av_seek_frame(m_formatContext, m_stream, dts, seek_forward ? 0 : AVSEEK_FLAG_BACKWARD) < 0)
  {
    qDebug() << "Failed to seek for time " << dts;
    return false;
  }

  avcodec_flush_buffers(m_codecContext);

  int got_frame = 0;
  AVPacket pkt{};
  AVFrame* f = get_new_frame();
  int k = 0;
  do
  {
    if (av_read_frame(m_formatContext, &pkt) == 0)
    {
      got_frame = enqueue_frame(&pkt, f);
      av_packet_unref(&pkt);
    }
    else
    {
      break;
    }
    k++;
  } while (k < 5 && !(got_frame && ((seek_forward && f->pkt_dts >= dts) || (!seek_forward && f->pkt_dts <= dts) || std::abs(f->pkt_dts - dts) < min_dts_delta)));

  m_last_dts = f->pkt_dts;
  m_framesToPlayer.enqueue(f);
  m_discardUntil = f;
  return true;
}

AVFrame* VideoDecoder::read_frame_impl() noexcept
{
  AVFrame* res = nullptr;

  if (m_stream != -1)
  {
    AVFrame* frame = get_new_frame();
    AVPacket packet;
    memset(&packet, 0, sizeof(AVPacket));
    bool ok = false;

    while (av_read_frame(m_formatContext, &packet) >= 0)
    {
      if (packet.stream_index == m_stream)
      {
        if (enqueue_frame(&packet, frame))
        {
          res = frame;
          ok = true;
        }

        av_packet_unref(&packet);
        packet = AVPacket{};
        break;
      }

      av_packet_unref(&packet);
      packet = AVPacket{};
    }

    if (!ok)
    {
      av_frame_free(&frame);
      res = nullptr;
    }
  }
  return res;
}

bool VideoDecoder::open_stream() noexcept
{
  bool res = false;

  if (!m_formatContext)
    return res;

  m_stream = -1;

  for (unsigned int i = 0; i < m_formatContext->nb_streams; i++)
  {
    if (m_formatContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
    {
      if(m_stream == -1)
      {
        m_stream = i;
        continue;
      }
    }
    m_formatContext->streams[i]->discard = AVDISCARD_ALL;
  }

  if(m_stream != -1)
  {
    const AVStream* stream =  m_formatContext->streams[m_stream];
    m_codecContext = stream->codec;
    const AVRational tb = stream->time_base;
    dts_per_flicks = (tb.den / (tb.num * ossia::flicks_per_second<double>));
    flicks_per_dts = (tb.num * ossia::flicks_per_second<double>) / tb.den;

    m_codec = avcodec_find_decoder(m_codecContext->codec_id);

    if (m_codec)
    {
      pixel_format = m_codecContext->pix_fmt;
      res = !(avcodec_open2(m_codecContext, m_codec, nullptr) < 0);
      width = m_codecContext->coded_width;
      height = m_codecContext->coded_height;
      fps = av_q2d(stream->avg_frame_rate);
    }
  }

  if (!res)
  {
    close_video();
  }
  return res;
}

void VideoDecoder::close_video() noexcept
{
  if (m_codecContext)
  {
    avcodec_close(m_codecContext);
    m_codecContext = nullptr;
    m_codec = nullptr;

    m_stream = -1;
  }
}

bool VideoDecoder::enqueue_frame(const AVPacket* pkt, AVFrame* frame) noexcept
{
  return readVideoFrame(m_codecContext, pkt, frame);
}

bool readVideoFrame(AVCodecContext* codecContext, const AVPacket* pkt, AVFrame* frame)
{
  int got_picture_ptr = 0;

  if (codecContext && pkt && frame)
  {
    int ret = avcodec_send_packet(codecContext, pkt);
    if (ret < 0)
      return ret == AVERROR_EOF ? 0 : ret;

    ret = avcodec_receive_frame(codecContext, frame);
    if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)
      return ret;

    if (ret >= 0)
      got_picture_ptr = 1;
  }

  return got_picture_ptr == 1;
}

}
