#pragma once
#include <Media/Libav.hpp>
#if SCORE_HAS_LIBAV
#include <Video/VideoInterface.hpp>
#include <Video/FrameQueue.hpp>
#include <Video/Rescale.hpp>
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
#include <vector>

#include <condition_variable>

namespace Video
{
class SCORE_PLUGIN_MEDIA_EXPORT VideoDecoder final : public VideoInterface
{
public:
  VideoDecoder() noexcept;
  ~VideoDecoder() noexcept;

  std::shared_ptr<VideoDecoder> clone() const noexcept;
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
  ReadFrame enqueue_frame(const AVPacket* pkt, AVFramePointer frame) noexcept;
  ReadFrame read_one_frame(AVFramePointer frame, AVPacket& packet);
  void init_scaler() noexcept;

  static const constexpr int frames_to_buffer = 16;

  std::string m_inputFile;

  std::thread m_thread;
  std::mutex m_condMut;
  std::condition_variable m_condVar;

  FrameQueue m_frames;

  AVFormatContext* m_formatContext{};
  AVCodecContext* m_codecContext{};
  AVStream* m_avstream{};
  const AVCodec* m_codec{};

  Rescale m_rescale;
  int m_stream{-1};

  int64_t m_duration{}; // in flicks

  std::atomic_int64_t m_seekTo = -1;
  std::atomic_int64_t m_last_dequeued_dts = 0;
  std::atomic_int64_t m_dequeued = 0;

  std::atomic_bool m_running{};
};

}
#endif
