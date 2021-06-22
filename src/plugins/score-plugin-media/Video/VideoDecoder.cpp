#include <Media/Libav.hpp>
#if SCORE_HAS_LIBAV

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}
#include "VideoDecoder.hpp"

#include <score/tools/Debug.hpp>

#include <ossia/detail/flicks.hpp>

#include <QDebug>
#include <QElapsedTimer>

#include <functional>
namespace Video
{
static char global_errbuf[512];
VideoInterface::~VideoInterface() { }

VideoDecoder::VideoDecoder() noexcept { }

VideoDecoder::~VideoDecoder() noexcept
{
  close_file();
}

std::shared_ptr<VideoDecoder> VideoDecoder::clone() const noexcept
{
  auto ptr = std::make_shared<VideoDecoder>();
  ptr->load(this->m_inputFile, {});
  return ptr;
}

bool VideoDecoder::load(
    const std::string& inputFile,
    double fps_unused) noexcept
{
  close_file();

  m_inputFile = inputFile;
  if (avformat_open_input(
          &m_formatContext, inputFile.c_str(), nullptr, nullptr)
      != 0)
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
  if (f)
  {
    m_last_dequeued_dts = f->pkt_dts;
  }
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
  if (m_releasedFrames.try_dequeue(f))
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

ReadFrame VideoDecoder::read_one_frame(AVFrame* frame, AVPacket& packet)
{
  const bool hap
      = (m_formatContext->streams[m_stream]->codecpar->codec_id
         == AV_CODEC_ID_HAP);
  int res{};
  while ((res = av_read_frame(m_formatContext, &packet)) >= 0)
  {
    if (packet.stream_index == m_stream)
    {
      if (hap)
      {
        auto cp = m_formatContext->streams[m_stream]->codecpar;
        // TODO this is a hack, we store the FOURCC in the format...

        memcpy(&frame->format, &cp->codec_tag, 4);

        frame->buf[0] = av_buffer_ref(packet.buf);
        frame->format = (cp->codec_tag);
        frame->best_effort_timestamp = packet.pts;
        frame->data[0]
            = packet.data; //(uint8_t*)malloc(sizeof(uint8_t) * packet.size);
        frame->linesize[0] = packet.size;
        frame->pts = packet.pts;
        frame->pkt_dts = packet.dts;
        frame->pkt_duration = packet.duration;

        //std::copy_n(packet.data, packet.size, frame->data[0]);
        return {frame, 0};
      }
      else
      {
        SCORE_ASSERT(m_codecContext);
        return enqueue_frame(&packet, &frame);
      }

      av_packet_unref(&packet);
      break;
    }

    av_packet_unref(&packet);
  }
  if (res != 0 && res != AVERROR_EOF)
    qDebug() << "Error while reading a frame: "
             << av_make_error_string(
                    global_errbuf, sizeof(global_errbuf), res);
  return {nullptr, res};
}

/*
// https://stackoverflow.com/a/44468529/1495627
static
int seek_to_frame(AVFormatContext* format, AVStream* stream, int frameIndex)
{
  using namespace std;
  // Seek is done on packet dts
  int64_t target_dts_usecs = std::round(frameIndex * (double)stream->r_frame_rate.den / stream->r_frame_rate.num * AV_TIME_BASE);
  // Remove first dts: when non zero seek should be more accurate
  auto first_dts_usecs = std::round(stream->first_dts * (double)stream->time_base.num / stream->time_base.den * AV_TIME_BASE);
  target_dts_usecs += first_dts_usecs;
  return av_seek_frame(format, -1, target_dts_usecs, AVSEEK_FLAG_BACKWARD);
}
*/

bool VideoDecoder::seek_impl(int64_t flicks) noexcept
{
  if (m_stream >= int(m_formatContext->nb_streams))
    return false;

  const auto stream = m_formatContext->streams[m_stream];
  const int64_t dts = flicks * dts_per_flicks;

  // Don't seek if we're less than 0.2 second close to the request
  // unit of the timestamps in seconds: stream->time_base.num / stream->time_base.den
  const int64_t min_dts_delta
      = (0.2 * stream->time_base.den) / stream->time_base.num;
  if (std::abs(dts - m_last_dequeued_dts) <= min_dts_delta)
    return false;

  // TODO - maybe we should also store the "last dequeued dts" from the
  // decoder side - this way no need to seek if we are in the interval
  const bool seek_forward = dts >= this->m_last_dequeued_dts;
  const auto start = stream->first_dts;
  if (av_seek_frame(
          m_formatContext, m_stream, start + dts, AVSEEK_FLAG_BACKWARD))
  {
    qDebug() << "Failed to seek for time " << dts;
    return false;
  }

  if (m_codecContext)
    avcodec_flush_buffers(m_codecContext);

  AVPacket pkt{};
  AVFrame* f = get_new_frame();
  assert(f);

  ReadFrame r;
  do
  {
    // First flush the buffer or smth
    do
    {
      r = read_one_frame(f, pkt);
    } while (r.error == AVERROR(EAGAIN));

    if (r.error == AVERROR_EOF)
    {
      break;
    }

    // we're starting to see correct frames, try to get close to the dts we want.
    while (f->pkt_dts + f->pkt_duration < dts)
    {
      r = read_one_frame(f, pkt);
      if (r.error == AVERROR_EOF)
        break;
    }
  } while (0);

  if (r.frame)
  {
    m_framesToPlayer.enqueue(f);
    m_discardUntil = f;
  }
  else
  {
    av_frame_free(&f);
  }
  return true;
}

AVFrame* VideoDecoder::read_frame_impl() noexcept
{
  ReadFrame res;

  if (m_stream != -1)
  {
    AVFrame* frame = get_new_frame();
    AVPacket packet;
    memset(&packet, 0, sizeof(AVPacket));

    do
    {
      res = read_one_frame(frame, packet);
    } while (res.error == AVERROR(EAGAIN));

    if (!res.frame)
    {
      av_frame_free(&frame);
      res.frame = nullptr;
    }
  }
  return res.frame;
}

void VideoDecoder::init_scaler() noexcept
{
  // Allocate a rescale context
  qDebug() << "allocating a rescaler for format"
           << av_get_pix_fmt_name(this->pixel_format);
  m_rescale = sws_getContext(
      this->width,
      this->height,
      this->pixel_format,
      this->width,
      this->height,
      AV_PIX_FMT_RGBA,
      SWS_FAST_BILINEAR,
      NULL,
      NULL,
      NULL);
  pixel_format = AV_PIX_FMT_RGBA;
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
      if (m_stream == -1)
      {
        m_stream = i;
        continue;
      }
    }
    m_formatContext->streams[i]->discard = AVDISCARD_ALL;
  }

  if (m_stream != -1)
  {
    const AVStream* stream = m_formatContext->streams[m_stream];
    const AVRational tb = stream->time_base;
    dts_per_flicks = (tb.den / (tb.num * ossia::flicks_per_second<double>));
    flicks_per_dts = (tb.num * ossia::flicks_per_second<double>) / tb.den;

    auto codecPar = stream->codecpar;
    if ((m_codec = avcodec_find_decoder(codecPar->codec_id)))
    {
      if (stream->codecpar->codec_id == AV_CODEC_ID_HAP)
      {
        // TODO this is a hack, we store the FOURCC in the format...
        memcpy(&pixel_format, &stream->codecpar->codec_tag, 4);
        width = codecPar->width;
        height = codecPar->height;
        fps = av_q2d(stream->avg_frame_rate);

        m_codecContext = nullptr;
        m_codec = nullptr;
        res = true;
      }
      else
      {
        pixel_format = (AVPixelFormat)codecPar->format;
        width = codecPar->width;
        height = codecPar->height;
        fps = av_q2d(stream->avg_frame_rate);
        m_codecContext = stream->codec;
        res = !(avcodec_open2(m_codecContext, m_codec, nullptr) < 0);

        switch (pixel_format)
        {
          // Supported formats for gpu decoding
          case AV_PIX_FMT_YUV420P:
          case AV_PIX_FMT_YUVJ420P:
          case AV_PIX_FMT_YUVJ422P:
          case AV_PIX_FMT_YUV422P:
          case AV_PIX_FMT_RGB0:
          case AV_PIX_FMT_RGBA:
          case AV_PIX_FMT_BGR0:
          case AV_PIX_FMT_BGRA:
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(56, 19, 100)
          case AV_PIX_FMT_GRAYF32LE:
          case AV_PIX_FMT_GRAYF32BE:
#endif
          case AV_PIX_FMT_GRAY8:
            break;
          // Other formats get rgb'd
          default:
          {
            init_scaler();
            break;
          }
        }
      }
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
  }

  if (m_rescale)
  {
    sws_freeContext(m_rescale);
    m_rescale = nullptr;
  }

  m_stream = -1;
}

ReadFrame
VideoDecoder::enqueue_frame(const AVPacket* pkt, AVFrame** frame) noexcept
{
  ReadFrame read = readVideoFrame(m_codecContext, pkt, *frame);
  if (read.frame)
  {
    if (m_rescale)
    {
      // alloc an rgb frame
      auto m_rgb = av_frame_alloc();
      m_rgb->width = this->width;
      m_rgb->height = this->height;
      m_rgb->format = AV_PIX_FMT_RGBA;
      av_frame_get_buffer(m_rgb, 0);

      // 2. Resize
      sws_scale(
          m_rescale,
          (*frame)->data,
          (*frame)->linesize,
          0,
          this->height,
          m_rgb->data,
          m_rgb->linesize);

      av_frame_free(frame);
      *frame = m_rgb;
    }
  }
  return read;
}

ReadFrame readVideoFrame(
    AVCodecContext* codecContext,
    const AVPacket* pkt,
    AVFrame* frame)
{
  if (codecContext && pkt && frame)
  {
    int ret = avcodec_send_packet(codecContext, pkt);
    if (ret < 0)
    {
      if (ret != AVERROR_EOF)
      {
        qDebug() << "avcodec_send_packet: "
                 << av_make_error_string(
                        global_errbuf, sizeof(global_errbuf), ret);
      }
      return {nullptr, ret};
    }

    ret = avcodec_receive_frame(codecContext, frame);
    if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)
    {
      qDebug() << "avcodec_receive_frame: "
               << av_make_error_string(
                      global_errbuf, sizeof(global_errbuf), ret);
      return {nullptr, ret};
    }
    else
    {
      if (frame->pts >= 0)
        return {frame, ret};
      else
        return {nullptr, ret};
    }
  }

  return {nullptr, AVERROR_UNKNOWN};
}

}
#endif
