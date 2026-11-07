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
class SCORE_PLUGIN_MEDIA_EXPORT VideoDecoder final
    : public VideoInterface
    , public LibAVDecoder
{
public:
  explicit VideoDecoder(DecoderConfiguration) noexcept;
  ~VideoDecoder() noexcept;

  bool open(const std::string& inputFile) noexcept;
  bool load(const std::string& inputFile) noexcept;

  const std::string& file() const noexcept { return m_inputFile; }

  int64_t duration() const noexcept;

  void seek(int64_t flicks) override;

  AVFrame* dequeue_frame() noexcept override;
  void release_frame(AVFrame*) noexcept override;

  //! While the returned object lives the decoding thread takes no seek
  //! request, and keeps decoding: the requests made meanwhile replace one
  //! another, and the last one is taken once it is destroyed.
  struct SeekHold
  {
    VideoDecoder* decoder{};
    SeekHold(VideoDecoder* d) noexcept
        : decoder{d}
    {
    }
    SeekHold(const SeekHold&) = delete;
    SeekHold& operator=(const SeekHold&) = delete;
    ~SeekHold()
    {
      {
        std::lock_guard l{decoder->m_condMut};
        decoder->m_holdSeeks = false;
      }
      decoder->m_condVar.notify_one();
    }
  };
  [[nodiscard]] SeekHold holdSeekRequests() noexcept
  {
    std::lock_guard l{m_condMut};
    m_holdSeeks = true;
    return SeekHold{this};
  }

private:
  void buffer_thread() noexcept;
  void close_file() noexcept;
  bool seek_impl(int64_t dts) noexcept;
  AVFrame* read_frame_impl() noexcept;
  bool open_stream() noexcept;
  void close_video() noexcept;

  static const constexpr int frames_to_buffer = 16;

  std::string m_inputFile;

  std::thread m_thread;
  std::mutex m_condMut;
  std::condition_variable m_condVar;
  //! holdSeekRequests; read and written under m_condMut.
  bool m_holdSeeks{};

  int64_t m_duration{}; // in flicks

  std::atomic_int64_t m_seekTo = -1;
  //! The display timestamp of the frame the consumer last took.
  std::atomic_int64_t m_last_dequeued_pts = AV_NOPTS_VALUE;
  std::atomic_int64_t m_dequeued = 0;

  std::atomic_bool m_running{};
};

}
#endif
