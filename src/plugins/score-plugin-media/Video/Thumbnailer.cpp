#include <Video/Thumbnailer.hpp>
#include <Video/VideoDecoder.hpp>
#include <ossia-qt/invoke.hpp>
#include <ossia/detail/flicks.hpp>
extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}
#include <QDebug>
#include <score/tools/Debug.hpp>
#include <wobjectimpl.h>

W_OBJECT_IMPL(Video::VideoThumbnailer)
namespace Video
{

VideoThumbnailer::VideoThumbnailer(QString path)
{
  connect(this, &VideoThumbnailer::requestThumbnails,
          this, &VideoThumbnailer::onRequest, Qt::QueuedConnection);

  auto inputFile = path.toUtf8();
  if (int err = avformat_open_input(&m_formatContext, inputFile.data(), nullptr, nullptr);
      err != 0)
  {
    char str[1500];
    qDebug() << "VideoThumbnailer: avformat_open_input failed" << av_make_error_string(str, 1499, err);

    SCORE_ASSERT(m_formatContext == nullptr);
    return;
  }

  if (int err = avformat_find_stream_info(m_formatContext, nullptr); err < 0)
  {
    char str[1500];
    qDebug() << "VideoThumbnailer: avformat_find_stream_info failed" << av_make_error_string(str, 1499, err);

    avformat_close_input(&m_formatContext);
    m_formatContext = nullptr;
    return;
  }

  bool res = false;
  for (unsigned int i = 0; i < m_formatContext->nb_streams; i++)
  {
    if (m_formatContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
    {
      m_stream = i;
      m_codecContext = m_formatContext->streams[i]->codec;
      const AVRational tb = m_formatContext->streams[m_stream]->time_base;
      dts_per_flicks = (tb.den / (tb.num * ossia::flicks_per_second<double>));
      flicks_per_dts = (tb.num * ossia::flicks_per_second<double>) / tb.den;

      m_codec = avcodec_find_decoder(m_codecContext->codec_id);

      if (m_codec)
      {
        pixel_format = m_codecContext->pix_fmt;
        int err = avcodec_open2(m_codecContext, m_codec, nullptr) ;
        res = !(err < 0);
        if(!res)
        {
          char str[1500];
          qDebug() << "VideoThumbnailer: avcodec_open2 failed" << av_make_error_string(str, 1499, err);
          break;
        }

        width = m_codecContext->coded_width;
        height = m_codecContext->coded_height;
        if(height > 0 && width > 0)
          m_aspect = double(width) / height;
        else
          m_aspect = 1.;

        smallHeight = 55;
        smallWidth = smallHeight * m_aspect;

        // Allocate a rescale context
        m_rescale = sws_getContext(
              this->width, this->height, this->pixel_format,
              smallWidth, smallHeight,
              AV_PIX_FMT_RGB24, SWS_FAST_BILINEAR, NULL, NULL, NULL);

        // Allocate a frame to do the RGB conversion
        m_rgb = av_frame_alloc();
        m_rgb->width = smallWidth;
        m_rgb->height = smallHeight;
        m_rgb->format = AV_PIX_FMT_RGB24;
        av_frame_get_buffer(m_rgb, 0);

        fps = av_q2d(m_formatContext->streams[i]->avg_frame_rate);
      }

      break;
    }
  }

  if(!res)
  {
    // error
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
      avcodec_close(m_codecContext);
      m_codecContext = nullptr;
      m_codec = nullptr;
    }

    if(m_formatContext)
    {
      avformat_close_input(&m_formatContext);
      m_formatContext = nullptr;
    }
  }
}

VideoThumbnailer::~VideoThumbnailer()
{

}

void VideoThumbnailer::onRequest(QVector<int64_t> flicks)
{
  if(!m_codecContext)
    return;

  m_requests = std::move(flicks);
  m_currentIndex = 0;

  if(m_currentIndex < m_requests.size())
  {
    ossia::qt::run_async(this, [this] { processNext(); });
  }
}

QImage VideoThumbnailer::process(int64_t flicks)
{
  AVFrame* res{};
  bool ok{};
  // 1. Seek
  {
    const int64_t dts = flicks * dts_per_flicks;

    int flags = AVSEEK_FLAG_FRAME;
    const bool seek_forward = dts >= this->m_last_dts;
    if (!seek_forward)
    {
      flags |= AVSEEK_FLAG_BACKWARD;
    }

    if (av_seek_frame(m_formatContext, m_stream, dts, flags))
    {
      qDebug() << "VideoThumbnailer: Failed to seek for time " << dts;
      return {};
    }

    avcodec_flush_buffers(m_codecContext);

    //int got_frame = 0;
    AVPacket packet{};
    memset(&packet, 0, sizeof(AVPacket));

    AVFrame* frame = av_frame_alloc();

    while (av_read_frame(m_formatContext, &packet) >= 0)
    {
      if (packet.stream_index == m_stream)
      {
        if (readVideoFrame(m_codecContext, &packet, frame))
        {
          if (frame->pkt_dts >= dts)
          {
            res = frame;
            ok = true;

            av_packet_unref(&packet);
            packet = AVPacket{};
            break;
          }
        }
      }

      av_packet_unref(&packet);
      packet = AVPacket{};
    }
    /*
    do
    {
      if (av_read_frame(m_formatContext, &pkt) == 0)
      {
        got_frame = readVideoFrame(m_codecContext, &pkt, frame);
        av_packet_unref(&pkt);
      }
      else
      {
        break;
      }
    } while (!(got_frame && frame->pkt_dts >= dts));
    */

    m_last_dts = frame->pkt_dts;

    if(!res)
    {
      av_frame_free(&frame);
      return {};
    }
  }


  // 2. Resize
  sws_scale(m_rescale, res->data, res->linesize, 0, this->height, m_rgb->data, m_rgb->linesize);
  const int lineSize = m_rgb->linesize[0];

  // 3. Convert to QImage
  // TODO optimizeme - try to see if we can skip m_rgb...
  QImage img{QSize(smallWidth, smallHeight), QImage::Format_RGB888};
  for(int y = 0; y < smallHeight; ++y)
  {
    std::copy_n(m_rgb->data[0] + y * lineSize, smallWidth * 3, img.scanLine(y));
  }

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
    thumbnailReady(flicks, std::move(img));

  m_currentIndex++;
  if(m_currentIndex < m_requests.size())
  {
    ossia::qt::run_async(this, [this] { processNext(); });
  }
}
}
