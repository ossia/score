#include <Media/Libav.hpp>
#if SCORE_HAS_LIBAV

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}
#include "VideoDecoder.hpp"

#include <Video/GpuFormats.hpp>

#include <score/tools/Debug.hpp>

#include <ossia/detail/flicks.hpp>

#include <QDebug>
#include <QElapsedTimer>

#include <functional>
namespace Video
{
static char global_errbuf[512];
VideoInterface::~VideoInterface() { }

void FreeAVFrame::operator()(AVFrame* f) const noexcept
{
  av_frame_free(&f);
}

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

bool VideoDecoder::load(const std::string& inputFile, double fps_unused) noexcept
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
  auto f = m_frames.discard_and_dequeue_one();
  if(f)
  {
    m_last_dequeued_dts = f->pkt_dts;
  }
  m_condVar.notify_one();
  return f;
}

void VideoDecoder::release_frame(AVFrame* frame) noexcept
{
  m_frames.release(frame);
}

void VideoDecoder::buffer_thread() noexcept
{
  while(m_running.load(std::memory_order_acquire))
  {
    if(int64_t seek = m_seekTo.exchange(-1); seek >= 0)
    {
      seek_impl(seek);
    }
    else
    {
      std::unique_lock lck{m_condMut};
      m_condVar.wait(lck, [&] {
        return m_frames.size() < frames_to_buffer / 2
               || !m_running.load(std::memory_order_acquire) || (m_seekTo != -1);
      });
      if(!m_running.load(std::memory_order_acquire))
        return;

      if(int64_t seek = m_seekTo.exchange(-1); seek >= 0)
      {
        seek_impl(seek);
      }

      if(m_frames.size() < (frames_to_buffer / 2))
      {
        if(auto f = read_frame_impl())
        {
          m_frames.enqueue(f);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(4));
      }
    }
  }
}

void VideoDecoder::close_file() noexcept
{
  // Stop the running status
  m_running.store(false, std::memory_order_release);
  m_condVar.notify_one();

  if(m_thread.joinable())
    m_thread.join();

  // Clear the stream
  close_video();

  // Clear the fmt context
  if(m_formatContext)
  {
    avformat_close_input(&m_formatContext);
    m_formatContext = nullptr;
  }

  // Remove frames that were in flight
  m_frames.drain();
}

ReadFrame VideoDecoder::read_one_frame(AVFramePointer frame, AVPacket& packet)
{
  const bool hap
      = (m_formatContext->streams[m_stream]->codecpar->codec_id == AV_CODEC_ID_HAP);
  int res{};
  if(hap && frame->buf[0])
  {
    av_buffer_unref(&frame->buf[0]);
  }
  while((res = av_read_frame(m_formatContext, &packet)) >= 0)
  {
    if(packet.stream_index == m_stream)
    {
      if(hap)
      {
        auto cp = m_formatContext->streams[m_stream]->codecpar;
        // TODO this is a hack, we store the FOURCC in the format...

        memcpy(&frame->format, &cp->codec_tag, 4);

        frame->buf[0] = av_buffer_ref(packet.buf);
        frame->width = cp->width;
        frame->height = cp->height;
        frame->format = (cp->codec_tag);
        frame->best_effort_timestamp = packet.pts;
        frame->data[0] = packet.data;
        frame->linesize[0] = packet.size;
        frame->pts = packet.pts;
        frame->pkt_dts = packet.dts;
        frame->pkt_duration = packet.duration;

        av_packet_unref(&packet);

        return {frame.release(), 0};
      }
      else
      {
        SCORE_ASSERT(m_codecContext);

        av_packet_rescale_ts(
            &packet, this->m_avstream->time_base, this->m_codecContext->time_base);

        auto res = enqueue_frame(&packet, std::move(frame));

        av_packet_unref(&packet);
        return res;
      }

      av_packet_unref(&packet);
      break;
    }

    av_packet_unref(&packet);
  }

  if(res != 0 && res != AVERROR_EOF)
  {
    qDebug() << "Error while reading a frame: "
             << av_make_error_string(global_errbuf, sizeof(global_errbuf), res);
  }
  av_packet_unref(&packet);
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

static int64_t to_av_time_base(AVRational tb, int64_t dts)
{
  constexpr auto av_tb = AVRational{1, AV_TIME_BASE};
  return av_rescale_q(dts, tb, av_tb);
}

bool VideoDecoder::seek_impl(int64_t flicks) noexcept
{
  if(m_stream >= int(m_formatContext->nb_streams))
    return false;

  const auto stream = m_formatContext->streams[m_stream];
  // Seeking with stream == -1 means that it is done AV_TIME_BASE
  constexpr auto av_tb = AVRational{1, AV_TIME_BASE};
  constexpr auto av_dts_per_flicks
      = (av_tb.den / (av_tb.num * ossia::flicks_per_second<double>));

  const auto dts = flicks * av_dts_per_flicks;

  const auto codec_tb = m_codecContext ? m_codecContext->time_base : stream->time_base;

  // Don't seek if we're less than 0.2 second close to the request
  // unit of the timestamps in seconds: stream->time_base.num / stream->time_base.den

  // qDebug() << "Codec pkt_timebase: " << m_codecContext->pkt_timebase.num
  //          << m_codecContext->pkt_timebase.den;
  // qDebug() << "Codec timebase: " << m_codecContext->time_base.num
  //          << m_codecContext->time_base.den;
  // qDebug() << "Stream timebase: " << stream->time_base.num << stream->time_base.den;
  // qDebug() << "AV timebase: " << av_tb.num << av_tb.den;
  const auto last_av_dts = to_av_time_base(codec_tb, m_last_dequeued_dts);
  // qDebug() << "????" << m_last_dequeued_dts << last_av_dts;
  const int64_t min_dts_delta = (0.2 * av_tb.den) / av_tb.num;
  // qDebug() << AV_TIME_BASE << min_dts_delta << dts << last_av_dts << dts - last_av_dts
  //          << (std::abs(dts - last_av_dts) <= min_dts_delta);
  if(std::abs(dts - last_av_dts) <= min_dts_delta)
  {
    // Let's always ensure that we seek to zero when asked no matter what
    if(dts != 0)
    {
      return false;
    }
  }

  // TODO - maybe we should also store the "last dequeued dts" from the
  // decoder side - this way no need to seek if we are in the interval
  // const bool seek_forward = dts >= this->m_last_dequeued_dts;
#if LIBAVFORMAT_VERSION_MAJOR >= 59
  const int64_t start = 0;
#else
  const int64_t start = stream->first_dts;
#endif

  if(avformat_seek_file(m_formatContext, -1, INT64_MIN, start + dts, INT64_MAX, 0))
  {
    qDebug() << "Failed to seek for time " << dts;
    return false;
  }

  if(m_codecContext)
    avcodec_flush_buffers(m_codecContext);

  AVPacket pkt{};

  ReadFrame r;
  do
  {
    // First flush the buffer or smth
    do
    {
      r = read_one_frame(m_frames.newFrame(), pkt);
    } while(r.error == AVERROR(EAGAIN));

    if(r.error == AVERROR_EOF || !r.frame)
    {
      break;
    }

    /*
    // Rescale the packet's dts into AV_TIME_BASE
    auto max_dts = r.frame->pkt_dts + r.frame->pkt_duration;
    auto max_av_dts = to_av_time_base(codec_tb, max_dts);
    //av_rescale_q(max_dts, stream->time_base, tb);
    // we're starting to see correct frames, try to get close to the dts we want.
    while(max_av_dts < dts)
    {
      r = read_one_frame(AVFramePointer{r.frame}, pkt);
      if(r.error == AVERROR_EOF || !r.frame)
        break;
    }
    */
  } while(0);

  if(r.frame)
  {
    m_frames.set_discard_frame(r.frame);
    m_frames.enqueue(r.frame);
  }
  else
  {
    av_frame_free(&r.frame);
  }
  return true;
}

AVFrame* VideoDecoder::read_frame_impl() noexcept
{
  ReadFrame res;

  if(m_stream != -1)
  {
    AVPacket packet;
    memset(&packet, 0, sizeof(AVPacket));

    do
    {
      res = read_one_frame(m_frames.newFrame(), packet);
    } while(res.error == AVERROR(EAGAIN));
  }
  return res.frame;
}

void VideoDecoder::init_scaler() noexcept
{
  m_rescale.open(*this);
  pixel_format = AV_PIX_FMT_RGBA;
}

bool VideoDecoder::open_stream() noexcept
{
  bool res = false;

  if(!m_formatContext)
    return res;

  m_stream = -1;

  for(unsigned int i = 0; i < m_formatContext->nb_streams; i++)
  {
    if(m_formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
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
    AVStream* stream = m_formatContext->streams[m_stream];
    m_avstream = stream;
    const AVRational tb = stream->time_base;
    dts_per_flicks = (tb.den / (tb.num * ossia::flicks_per_second<double>));
    flicks_per_dts = (tb.num * ossia::flicks_per_second<double>) / tb.den;

    auto codecPar = stream->codecpar;
    if((m_codec = avcodec_find_decoder(codecPar->codec_id)))
    {
      if(codecPar->width <= 0 || codecPar->height <= 0)
      {
        qDebug() << "VideoDecoder: invalid video: width or height is 0";
        res = false;
      }
      else
      {
        if(stream->codecpar->codec_id == AV_CODEC_ID_HAP)
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

          m_codecContext = avcodec_alloc_context3(m_codec);
          avcodec_parameters_to_context(m_codecContext, stream->codecpar);

          m_codecContext->framerate = av_guess_frame_rate(m_formatContext, stream, NULL);
          m_codecContext->pkt_timebase = stream->time_base;
          m_codecContext->codec_id = m_codec->id;
          res = !(avcodec_open2(m_codecContext, m_codec, nullptr) < 0);

          if(m_codecContext)
          {
            auto tb = m_codecContext->time_base;
            dts_per_flicks = (tb.den / (tb.num * ossia::flicks_per_second<double>));
            flicks_per_dts = (tb.num * ossia::flicks_per_second<double>) / tb.den;
          }

          if(Video::formatNeedsDecoding(pixel_format))
          {
            init_scaler();
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

void VideoDecoder::close_video() noexcept
{
  if(m_codecContext)
  {
    avcodec_close(m_codecContext);
    avcodec_free_context(&m_codecContext);
    m_codecContext = nullptr;
    m_codec = nullptr;
  }

  m_rescale.close();

  m_avstream = nullptr;
  m_stream = -1;
}

ReadFrame VideoDecoder::enqueue_frame(const AVPacket* pkt, AVFramePointer frame) noexcept
{
  ReadFrame read = readVideoFrame(m_codecContext, pkt, frame.get());
  if(!read.frame)
  {
    this->m_frames.enqueue_decoding_error(frame.release());
    return read;
  }

  if(m_rescale)
  {
    m_rescale.rescale(*this, m_frames, frame, read);
  }
  else
  {
    // it is already stored in "read" but well
    read.frame = frame.release();
  }
  return read;
}

ReadFrame
readVideoFrame(AVCodecContext* codecContext, const AVPacket* pkt, AVFrame* frame)
{
  if(codecContext && pkt && frame)
  {
    int ret = avcodec_send_packet(codecContext, pkt);
    if(ret < 0)
    {
      if(ret != AVERROR_EOF)
      {
        qDebug() << "avcodec_send_packet: "
                 << av_make_error_string(global_errbuf, sizeof(global_errbuf), ret);
      }
      return {nullptr, ret};
    }

    ret = avcodec_receive_frame(codecContext, frame);
    if(ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)
    {
      qDebug() << "avcodec_receive_frame: "
               << av_make_error_string(global_errbuf, sizeof(global_errbuf), ret);
      return {nullptr, ret};
    }
    else
    {
      if(frame->pts >= 0)
        return {frame, ret};
      else
        return {nullptr, ret};
    }
  }

  return {nullptr, AVERROR_UNKNOWN};
}

}
#endif
