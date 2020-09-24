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

class SCORE_PLUGIN_MEDIA_EXPORT VideoDecoder final : public VideoInterface
{
public:
  VideoDecoder() noexcept;
  ~VideoDecoder() noexcept;

  bool load(const std::string& inputFile, double fps_unused) noexcept;

  int64_t duration() const noexcept;

  void seek(int64_t flicks);

  AVFrame* dequeue_frame() noexcept override;
  void release_frame(AVFrame*) noexcept override;

private:
  void buffer_thread() noexcept;
  void close_file() noexcept;
  bool seek_impl(int64_t dts) noexcept;
  AVFrame* read_frame_impl() noexcept;
  bool open_stream() noexcept;
  void close_video() noexcept;
  bool enqueue_frame(const AVPacket* pkt, AVFrame* frame) noexcept;
  AVFrame* get_new_frame() noexcept;
  void drain_frames() noexcept;

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

  int64_t m_duration{}; // in flicks

  std::atomic<AVFrame*> m_discardUntil{};
  std::atomic_int64_t m_seekTo = -1;
  int64_t m_last_dts = 0;

  std::atomic_bool m_running{};
};

bool readVideoFrame(AVCodecContext* codecContext, const AVPacket* pkt, AVFrame* frame);
}
