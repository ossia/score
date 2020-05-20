#pragma once
#include <Video/VideoInterface.hpp>
extern "C"
{
#include <libavformat/avformat.h>
}

#include <readerwriterqueue.h>
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
  bool enqueue_frame(const AVPacket* pkt, AVFrame* frame) noexcept;
  AVFrame* get_new_frame() noexcept;

  static const constexpr int frames_to_buffer = 16;

  std::thread m_thread;
  std::mutex m_condMut;
  std::condition_variable m_condVar;

  moodycamel::ReaderWriterQueue<AVFrame*, 16> m_framesToPlayer;
  moodycamel::ReaderWriterQueue<AVFrame*, 16> m_releasedFrames;

  AVFormatContext* m_formatContext{};
  AVCodecContext* m_codecContext{};
  AVCodec* m_codec{};
  int m_stream{-1};

  std::atomic_bool m_running{};
};

}
