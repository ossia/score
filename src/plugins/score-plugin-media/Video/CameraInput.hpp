#pragma once
#include <Video/VideoInterface.hpp>
#include <Video/FrameQueue.hpp>
extern "C"
{
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

#include <ossia/detail/lockfree_queue.hpp>
#include <score_plugin_media_export.h>

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

#include <condition_variable>
namespace Video
{

class SCORE_PLUGIN_MEDIA_EXPORT CameraInput final : public VideoInterface
{
public:
  CameraInput() noexcept;
  ~CameraInput() noexcept;

  bool load(const std::string& inputDevice, const std::string& format, int w, int h, double fps) noexcept;

  bool start() noexcept;
  void stop() noexcept;

  AVFrame* dequeue_frame() noexcept override;
  void release_frame(AVFrame* frame) noexcept override;

private:
  void buffer_thread() noexcept;
  void close_file() noexcept;
  AVFrame* read_frame_impl() noexcept;
  bool open_stream() noexcept;
  void close_stream() noexcept;
  bool enqueue_frame(const AVPacket* pkt, AVFrame** frame) noexcept;
  void init_scaler() noexcept;

  static const constexpr int frames_to_buffer = 1;

  std::thread m_thread;
  FrameQueue m_frames;

  std::string m_inputKind;
  std::string m_inputDevice;
  AVFormatContext* m_formatContext{};
  AVCodecContext* m_codecContext{};
  SwsContext* m_rescale{};
  AVCodec* m_codec{};
  int m_stream{-1};

  std::atomic_bool m_running{};
};

}
