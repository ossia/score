#pragma once
extern "C"
{
#include <libavformat/avformat.h>
}

#include <string>
#include <atomic>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <readerwriterqueue.h>
#include <score_plugin_media_export.h>
namespace Video
{

class SCORE_PLUGIN_MEDIA_EXPORT VideoDecoder
{
public:
  VideoDecoder() noexcept;

  ~VideoDecoder() noexcept;

  bool load(const std::string& inputFile, double fps_unused) noexcept;

  int width() const noexcept;
  int height() const noexcept;
  double fps() const noexcept;
  int64_t duration() const noexcept;

  AVPixelFormat pixel_format() const noexcept;

  void seek(int64_t dts);

  AVFrame* dequeue_frame() noexcept;

private:
  std::mutex m_condMut;
  std::condition_variable m_condVar;
  void buffer_thread() noexcept;

  void close_file() noexcept;

  bool seek_impl(int64_t dts) noexcept;

  AVFrame* read_frame_impl() noexcept;

  bool open_stream() noexcept;

  void close_video() noexcept;

  bool enqueue_frame(const AVPacket* pkt, AVFrame* frame) noexcept;

  static const constexpr int frames_to_buffer = 16;

  std::thread m_thread;

  moodycamel::ReaderWriterQueue<AVFrame*> m_frames;
  //std::mutex m_framesMutex;

  AVFormatContext* m_formatContext{};
  AVCodecContext* m_codecContext{};
  AVCodec* m_codec{};
  int m_stream{-1};

  AVPixelFormat m_pixel_format{};

  int m_width{};
  int m_height{};
  double m_rate{};
  int64_t m_duration{}; // in flicks

  std::atomic<AVFrame*> m_discardUntil{};
  std::atomic_int64_t m_seekTo = -1;
  int64_t m_last_dts = 0;

  std::atomic_bool m_running{};
};

}
