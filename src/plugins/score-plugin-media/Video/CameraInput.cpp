
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}
#include "CameraInput.hpp"

#include <ossia/detail/flicks.hpp>

#include <QDebug>
#include <QElapsedTimer>
#include <fmt/format.h>
#include <functional>
namespace Video
{

CameraInput::CameraInput() noexcept { }

CameraInput::~CameraInput() noexcept
{
  close_file();
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

bool CameraInput::load(const std::string& inputKind, const std::string& inputDevice, int w, int h, double fps) noexcept
{
  close_file();

  auto ifmt = av_find_input_format(inputKind.c_str());
  assert(ifmt);

  m_formatContext = avformat_alloc_context();
  m_formatContext->flags |= AVFMT_FLAG_NONBLOCK;

  /* TODO it seems that things work without that
  AVDictionary *options = nullptr;
  av_dict_set(&options, "framerate", std::to_string((int)fps).c_str(), 0);
  av_dict_set(&options, "input_format", format.c_str(), 0); // this one seems failing
  av_dict_set(&options, "video_size", fmt::format("{}x{}", w, h).c_str(), 0);
  */
  if (avformat_open_input(&m_formatContext, inputDevice.c_str(), ifmt, nullptr) != 0)
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

  return true;
}

AVFrame* CameraInput::dequeue_frame() noexcept
{
  AVFrame* f{};
  m_framesToPlayer.try_dequeue(f);
  m_condVar.notify_one();
  return f;
}

void CameraInput::release_frame(AVFrame* frame) noexcept
{
  m_releasedFrames.enqueue(frame);
}

void CameraInput::buffer_thread() noexcept
{
  while (m_running.load(std::memory_order_acquire))
  {
    std::unique_lock lck{m_condMut};
    m_condVar.wait(lck, [&] {
      return m_framesToPlayer.size_approx() < frames_to_buffer
          || !m_running.load(std::memory_order_acquire);
    });
    if (!m_running.load(std::memory_order_acquire))
      return;

    if (m_framesToPlayer.size_approx() < (frames_to_buffer / 2))
      if (auto f = read_frame_impl())
      {
        m_framesToPlayer.enqueue(f);
      }
  }
}

void CameraInput::close_file() noexcept
{
  m_running.store(false, std::memory_order_release);
  m_condVar.notify_one();

  if (m_thread.joinable())
    m_thread.join();

  close_video();

  if (m_formatContext)
  {
    avformat_close_input(&m_formatContext);
    m_formatContext = nullptr;
  }
}

AVFrame* CameraInput::get_new_frame() noexcept
{
  AVFrame* f{};
  if(m_releasedFrames.try_dequeue(f))
    return f;
  return av_frame_alloc();
}

AVFrame* CameraInput::read_frame_impl() noexcept
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

bool CameraInput::open_stream() noexcept
{
  bool res = false;

  if (!m_formatContext)
    return res;

  m_stream = -1;

  for (unsigned int i = 0; i < m_formatContext->nb_streams; i++)
  {
    if (m_formatContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
    {
      m_stream = i;
      m_codecContext = m_formatContext->streams[i]->codec;
      m_codec = avcodec_find_decoder(m_codecContext->codec_id);

      if (m_codec)
      {
        pixel_format = m_codecContext->pix_fmt;
        res = !(avcodec_open2(m_codecContext, m_codec, nullptr) < 0);
        width = m_codecContext->coded_width;
        height = m_codecContext->coded_height;
        fps = av_q2d(m_formatContext->streams[i]->avg_frame_rate);
      }

      break;
    }
  }

  if (!res)
  {
    close_video();
  }

  return res;
}

void CameraInput::close_video() noexcept
{
  if (m_codecContext)
  {
    avcodec_close(m_codecContext);
    m_codecContext = nullptr;
    m_codec = nullptr;

    m_stream = -1;
  }
}

bool CameraInput::enqueue_frame(const AVPacket* pkt, AVFrame* frame) noexcept
{
  int got_picture_ptr = 0;

  if (m_codecContext && pkt && frame)
  {
    int ret = avcodec_send_packet(m_codecContext, pkt);
    if (ret < 0)
      return ret == AVERROR_EOF ? 0 : ret;

    ret = avcodec_receive_frame(m_codecContext, frame);
    if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)
      return ret;

    if (ret >= 0)
      got_picture_ptr = 1;
  }

  return got_picture_ptr == 1;
}

}
