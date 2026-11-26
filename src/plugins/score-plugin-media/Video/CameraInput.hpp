#pragma once
#include <Media/Libav.hpp>
#if SCORE_HAS_LIBAV

#include <Video/ExternalInput.hpp>
#include <Video/FrameQueue.hpp>
#include <Video/LibavInterrupt.hpp>
#include <Video/Rescale.hpp>

extern "C" {
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

#include <ossia/detail/lockfree_queue.hpp>

#include <score_plugin_media_export.h>

#include <atomic>
#include <string>
#include <thread>
namespace Video
{

class SCORE_PLUGIN_MEDIA_EXPORT CameraInput final
    : public ExternalInput
    , public LibAVDecoder
{
public:
  CameraInput() noexcept;
  ~CameraInput() noexcept;

  bool load(
      const std::string& inputDevice, const std::string& format, int w, int h,
      double fps, int codec, int pixelfmt) noexcept;

  bool start() noexcept override;
  void stop() noexcept override;

  AVFrame* dequeue_frame() noexcept override;
  void release_frame(AVFrame* frame) noexcept override;

  bool notifyFormatChange(
      int new_w, int new_h, AVPixelFormat new_pixfmt) noexcept override;

private:
  void buffer_thread() noexcept;
  void close_file() noexcept;
  struct Read
  {
    AVFrame* frame{};
    bool consumed{}; //!< the device gave at least one packet
  };
  Read read_frame_impl(bool decode) noexcept;
  bool read_newest_packet() noexcept;
  bool open_stream() noexcept;
  void close_stream() noexcept;
  bool alloc_format_context() noexcept;
  ReadFrame read_one_frame(AVFramePointer frame, AVPacket& packet);

  static const constexpr int frames_to_buffer = 1;
  static const constexpr std::size_t frames_to_queue = 4;
  //! Packets skipped at most to reach the newest one a device has buffered
  static const constexpr int max_backlog = 64;
  //! A read that waited this long found the device's buffer empty
  static const constexpr auto blocked_read = std::chrono::milliseconds(2);

  //! Opening a device that never answers must not wedge the caller
  static const constexpr auto open_timeout = std::chrono::milliseconds(2000);
  //! A camera that stopped delivering must not wedge the buffer thread
  static const constexpr auto read_timeout = std::chrono::milliseconds(5000);

  LibavInterrupt m_interrupt;

  AVCodecID m_requestedCodec{AV_CODEC_ID_NONE};
  AVPixelFormat m_requestedPixfmt{AV_PIX_FMT_NONE};

  std::thread m_thread;

  std::string m_inputKind;
  std::string m_inputDevice;

  std::atomic_bool m_running{};
  IdleConsumer m_idle;
  AVPacket* m_newest{};
  AVPacket* m_scratch{};
  bool m_intraOnly{};
  bool m_wasIdle{};
};

}
#endif
