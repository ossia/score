#pragma once
#include <Media/Libav.hpp>
#if SCORE_HAS_LIBAV
#include <Video/FrameQueue.hpp>
#include <Video/Rescale.hpp>
#include <Video/VideoInterface.hpp>
extern "C" {
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

#include <ossia/detail/lockfree_queue.hpp>

#include <score_plugin_media_export.h>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace Video
{
class SCORE_PLUGIN_MEDIA_EXPORT VideoDecoder
    : public VideoInterface
    , public LibAVDecoder
{
public:
  explicit VideoDecoder(DecoderConfiguration) noexcept;
  ~VideoDecoder() noexcept;

  bool open(const std::string& inputFile) noexcept;

  const std::string& file() const noexcept { return m_inputFile; }

  int64_t duration() const noexcept;

  void seek(int64_t flicks);
  void release_frame(AVFrame*) noexcept override;
  void read_next_frame();
  AVFrame* dequeue_frame() noexcept override;

  AVFrame* read_frame_impl() noexcept;
  bool seek_impl(int64_t dts) noexcept;

protected:
  bool open_stream() noexcept;
  void close_video() noexcept;
  void close() noexcept;

  static const constexpr int frames_to_buffer = 16;

  std::string m_inputFile;
  int64_t m_duration{}; // in flicks

  std::atomic_int64_t m_seekTo = -1;
  std::atomic_int64_t m_last_dequeued_dts = 0;
  std::atomic_int64_t m_dequeued = 0;
};

class SCORE_PLUGIN_MEDIA_EXPORT VideoDecoderThreaded : public VideoDecoder
{
public:
  using VideoDecoder::VideoDecoder;
  ~VideoDecoderThreaded() noexcept;
  bool load(const std::string& inputFile) noexcept;

  AVFrame* dequeue_frame() noexcept override;

private:
  void close_file() noexcept;
  void buffer_thread() noexcept;

  std::thread m_thread;
  std::mutex m_condMut;
  std::condition_variable m_condVar;

  std::atomic_bool m_running{};
};
}
#endif
