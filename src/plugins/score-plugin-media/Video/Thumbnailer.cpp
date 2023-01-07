#include <Media/Libav.hpp>
#if SCORE_HAS_LIBAV

#include <Video/Thumbnailer.hpp>
#include <Video/VideoDecoder.hpp>

#include <ossia/detail/flicks.hpp>

#include <ossia-qt/invoke.hpp>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}
#include <score/tools/Debug.hpp>

#include <ossia/detail/libav.hpp>

#include <QDebug>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Video::VideoThumbnailer)
namespace Video
{

VideoThumbnailer::VideoThumbnailer(QString path)
{
  connect(
      this, &VideoThumbnailer::requestThumbnails, this, &VideoThumbnailer::onRequest,
      Qt::QueuedConnection);

  auto inputFile = path.toUtf8();
  if(int err = avformat_open_input(&m_formatContext, inputFile.data(), nullptr, nullptr);
     err != 0)
  {
    char str[1500];
    qDebug() << "VideoThumbnailer: avformat_open_input failed"
             << av_make_error_string(str, 1499, err);

    SCORE_ASSERT(m_formatContext == nullptr);
    return;
  }

  if(int err = avformat_find_stream_info(m_formatContext, nullptr); err < 0)
  {
    char str[1500];
    qDebug() << "VideoThumbnailer: avformat_find_stream_info failed"
             << av_make_error_string(str, 1499, err);

    avformat_close_input(&m_formatContext);
    m_formatContext = nullptr;
    return;
  }

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

  bool res = false;
  if(m_stream != -1)
  {
    const AVStream* stream = m_formatContext->streams[m_stream];
    const AVRational tb = m_formatContext->streams[m_stream]->time_base;
    dts_per_flicks = (tb.den / (tb.num * ossia::flicks_per_second<double>));
    flicks_per_dts = (tb.num * ossia::flicks_per_second<double>) / tb.den;

    m_codec = avcodec_find_decoder(stream->codecpar->codec_id);

    if(m_codec)
    {
      pixel_format = (AVPixelFormat)stream->codecpar->format;
      width = stream->codecpar->width;
      height = stream->codecpar->height;
      fps = av_q2d(stream->avg_frame_rate);

      m_codecContext = avcodec_alloc_context3(nullptr);
      avcodec_parameters_to_context(m_codecContext, stream->codecpar);

      m_codecContext->pkt_timebase = stream->time_base;
      m_codecContext->codec_id = m_codec->id;
      int err = avcodec_open2(m_codecContext, m_codec, nullptr);
      res = !(err < 0);
      if(!res)
      {
        char str[1500];
        qDebug() << "VideoThumbnailer: avcodec_open2 failed"
                 << av_make_error_string(str, 1499, err);
        return;
      }

      width = m_codecContext->coded_width;
      height = m_codecContext->coded_height;
      if(height > 0 && width > 0)
      {
        m_aspect = double(width) / height;
      }
      else
      {
        qDebug() << "VideoThumbnailer: invalid video: width or height is 0";
        return;
      }

      smallHeight = 55;
      smallWidth = smallHeight * m_aspect;

      // Allocate a rescale context
      m_rescale = sws_getContext(
          this->width, this->height, this->pixel_format, smallWidth, smallHeight,
          AV_PIX_FMT_RGB24, SWS_FAST_BILINEAR, NULL, NULL, NULL);

      // Allocate a frame to do the RGB conversion
      m_rgb = av_frame_alloc();
      m_rgb->width = smallWidth;
      m_rgb->height = smallHeight;
      m_rgb->format = AV_PIX_FMT_RGB24;
      av_frame_get_buffer(m_rgb, 0);

      fps = av_q2d(stream->avg_frame_rate);
    }
  }
}

VideoThumbnailer::~VideoThumbnailer()
{
  if(m_rgb)
  {
    av_frame_free(&m_rgb);
    m_rgb = nullptr;
  }

  if(m_rescale)
  {
    sws_freeContext(m_rescale);
    m_rescale = nullptr;
  }

  if(m_codecContext)
  {
    avcodec_free_context(&m_codecContext);
    m_codecContext = nullptr;
    m_codec = nullptr;
  }

  if(m_formatContext)
  {
    avio_flush(m_formatContext->pb);
    avformat_flush(m_formatContext);
    avformat_close_input(&m_formatContext);
    m_formatContext = nullptr;
  }
}

void VideoThumbnailer::onRequest(int64_t req, QVector<int64_t> flicks)
{
  if(!m_codecContext)
    return;

  m_requests = std::move(flicks);
  m_requestIndex = req;
  m_currentIndex = 0;

  if(m_currentIndex < m_requests.size())
  {
    ossia::qt::run_async(this, [this] { processNext(); });
  }
}

QImage VideoThumbnailer::process(int64_t flicks)
{
  AVFramePointer res;
  // 1. Seek
  {
    const int64_t dts = flicks * dts_per_flicks;

    // TODO - maybe we should also store the "last dequeued dts" from the
    // decoder side - this way no need to seek if we are in the interval

    const bool seek_forward = dts >= this->m_last_dts;
    int flags = seek_forward ? 0 : AVSEEK_FLAG_BACKWARD;
    flags |= ossia::OSSIA_LIBAV_SEEK_ROUGH;
    if(!ossia::seek_to_flick(
           m_formatContext, m_codecContext, m_formatContext->streams[m_stream], flicks,
           flags))
    {
      qDebug() << "VideoThumbnailer: Failed to seek for time " << dts;
      return {};
    }

    AVFramePointer frame{av_frame_alloc()};

    AVPacket* packet = av_packet_alloc();

    int k = 0;
  retry_decode:
    av_packet_unref(packet);
    while(av_read_frame(m_formatContext, packet) >= 0 && k < 5)
    {
      k++;
      if(packet->stream_index != m_stream)
      {
        qDebug() << "packet.stream_index != m_stream";
        av_packet_unref(packet);
        goto retry_decode;
      }

      int ret = avcodec_send_packet(m_codecContext, packet);
      if(ret < 0)
      {
        if(ret == AVERROR(EAGAIN))
        {
          av_packet_unref(packet);
          goto retry_decode;
        }
        else if(ret == AVERROR_EOF)
        {
          qDebug("avcodec_send_packet eof");
          break;
        }
        else
        {
          qDebug("avcodec_send_packet error");
          break;
        }
      }

      ret = avcodec_receive_frame(m_codecContext, frame.get());
      if(ret < 0)
      {
        if(ret == AVERROR(EAGAIN))
        {
          av_packet_unref(packet);
          goto retry_decode;
        }
        else if(ret == AVERROR_EOF)
        {
          qDebug("avcodec_receive_frame eof");
        }
        else
        {
          qDebug("avcodec_receive_frame error");
        }
      }

      if(ret == 0)
      {
        if(frame->pkt_dts >= dts)
        {
          res = std::move(frame);

          av_packet_unref(packet);
          break;
        }
      }

      av_packet_unref(packet);
    }

    if(frame)
      m_last_dts = frame->pkt_dts;
    else if(res)
      m_last_dts = res->pkt_dts;

    av_packet_unref(packet);
    av_packet_free(&packet);
    if(!res)
    {
      return {};
    }
  }

  // 2. Resize
  QImage img{QSize(m_rgb->linesize[0] / 3, smallHeight), QImage::Format_RGB888};
  uint8_t* data[1] = {(uint8_t*)img.bits()};
  sws_scale(m_rescale, res->data, res->linesize, 0, this->height, data, m_rgb->linesize);

  return img;
}

void VideoThumbnailer::processNext()
{
  if(!m_codecContext)
    return;
  if(m_currentIndex >= m_requests.size())
    return;

  const auto flicks = m_requests[m_currentIndex];

  if(auto img = process(flicks); !img.isNull())
    thumbnailReady(m_requestIndex, flicks, std::move(img));

  m_currentIndex++;
  if(m_currentIndex < m_requests.size())
  {
    ossia::qt::run_async(this, [this] { processNext(); });
  }
}
}
#endif
