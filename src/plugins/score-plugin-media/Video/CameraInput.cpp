#include <Media/Libav.hpp>
#if SCORE_HAS_LIBAV
#include "CameraInput.hpp"
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}
#include <Video/GpuFormats.hpp>

#include <score/tools/Debug.hpp>
#include <ossia/detail/fmt.hpp>
#include <QDebug>
#include <QElapsedTimer>

#include <functional>
namespace Video
{

ExternalInput::~ExternalInput() = default;

CameraInput::CameraInput() noexcept
{
  realTime = true;
}

CameraInput::~CameraInput() noexcept
{
  close_file();
}

bool CameraInput::load(
    const std::string& inputKind, const std::string& inputDevice, int w, int h,
    double fps, int codec, int pixelfmt) noexcept
{
  close_file();
  m_inputKind = inputKind;
  m_inputDevice = inputDevice;

  this->width = w;
  this->height = h;
  this->fps = fps;
  this->m_requestedCodec = (AVCodecID)codec;
  this->m_requestedPixfmt = (AVPixelFormat)pixelfmt;

  auto ifmt = av_find_input_format(m_inputKind.c_str());
  return (bool)ifmt;
}

bool CameraInput::start() noexcept
{
  if(m_running)
    return false;

  auto ifmt = av_find_input_format(m_inputKind.c_str());
  if(!ifmt)
    return false;

  m_formatContext = avformat_alloc_context();
  m_formatContext->flags |= AVFMT_FLAG_NONBLOCK;
  m_formatContext->flags |= AVFMT_FLAG_NOBUFFER;

  AVDictionary* options = nullptr;

  if(auto codec_name = avcodec_get_name(this->m_requestedCodec))
    av_dict_set(&options, "input_format", codec_name, 0);

  // FIXME support pixel format choosing

  if(fps > 0.)
    av_dict_set_int(&options, "framerate", fps, 0);

  if(this->width > 0 && this->height > 0)
  {
    av_dict_set(
        &options, "video_size", fmt::format("{}x{}", this->width, this->height).c_str(),
        0);
  }

  int ret = avformat_open_input(&m_formatContext, m_inputDevice.c_str(), ifmt, &options);
  av_dict_free(&options);

  if(ret < 0)
  {
    qDebug() << "avformat_open_input" << av_to_string(ret);

    // Let's try with the default settings if the requested settings did not work:
    ret = avformat_open_input(&m_formatContext, m_inputDevice.c_str(), ifmt, nullptr);
    if(ret < 0)
    {
      qDebug() << "avformat_open_input" << av_to_string(ret);

      close_file();
      return false;
    }
  }

  ret = avformat_find_stream_info(m_formatContext, nullptr);
  if(ret < 0)
  {
    qDebug() << "avformat_find_stream_info" << av_to_string(ret);
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
  return true;
}

void CameraInput::stop() noexcept
{
  close_file();
}

AVFrame* CameraInput::dequeue_frame() noexcept
{
  return m_frames.dequeue();
}

void CameraInput::release_frame(AVFrame* frame) noexcept
{
  m_frames.release(frame);
}

void CameraInput::buffer_thread() noexcept
{
  while(m_running.load(std::memory_order_acquire))
  {
    if(m_frames.size() < 4)
    {
      if(auto f = read_frame_impl())
      {
        m_frames.enqueue(f);
      }
    }

    // Wait either half the expected framerate or 4 ms
    int wait_ms = std::clamp((this->fps > 0.) ? (1000. / fps) / 2. : 4, 1., 100.);
    std::this_thread::sleep_for(std::chrono::milliseconds(wait_ms));
  }
}

void CameraInput::close_file() noexcept
{
  // Stop the running status
  m_running.store(false, std::memory_order_release);

  if(m_thread.joinable())
    m_thread.join();

  // Clear the stream
  close_stream();

  // Clear the fmt context
  if(m_formatContext)
  {
    avformat_close_input(&m_formatContext);
    m_formatContext = nullptr;
  }

  m_rescale.close();

  // Remove frames that were in flight
  m_frames.drain();
}

AVFrame* CameraInput::read_frame_impl() noexcept
{
  ReadFrame res;

  if(m_avstream)
  {
    AVPacket packet;
    memset(&packet, 0, sizeof(AVPacket));

    do
    {
      res = read_one_frame_avcodec(m_frames.newFrame(), packet);
    } while(res.error == AVERROR(EAGAIN));
  }
  return res.frame;
}

bool CameraInput::open_stream() noexcept
{
  bool res = false;

  if(!m_formatContext)
  {
    close_stream();
    return false;
  }

  m_avstream = nullptr;

  for(unsigned int i = 0; i < m_formatContext->nb_streams; i++)
  {
    auto stream = m_formatContext->streams[i];
    auto codecPar = stream->codecpar;

    qDebug() << codecPar->codec_id;

    if((m_codec = avcodec_find_decoder(codecPar->codec_id)))
    {
      qDebug() << "Codec: " << m_codec->long_name << m_codec->name;
    }

    if(codecPar->codec_type == AVMEDIA_TYPE_VIDEO)
    {
      if((m_codec = avcodec_find_decoder(codecPar->codec_id)))
      {
        m_avstream = stream;
        pixel_format = static_cast<AVPixelFormat>(codecPar->format);
        width = codecPar->width;
        height = codecPar->height;
        fps = av_q2d(m_avstream->avg_frame_rate);

        res = open_codec_context(*this, stream, [=](AVCodecContext& ctx) {
          ctx.framerate = av_guess_frame_rate(m_formatContext, (AVStream*)stream, NULL);
          m_codecContext->flags |= AV_CODEC_FLAG_LOW_DELAY;
          m_codecContext->flags2 |= AV_CODEC_FLAG2_FAST;
        });
      }
    }
  }

  if(!res)
  {
    close_stream();
    return false;
  }

  return true;
}

void CameraInput::close_stream() noexcept
{
  if(m_codecContext)
  {
    avcodec_close(m_codecContext);
  }

  m_rescale.close();

  m_codecContext = nullptr;
  m_codec = nullptr;
  m_avstream = nullptr;
}
}
#endif
