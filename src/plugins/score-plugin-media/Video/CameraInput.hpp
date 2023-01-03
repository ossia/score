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
namespace Video
{

class SCORE_PLUGIN_MEDIA_EXPORT ExternalInput : public VideoInterface
{
public:
  virtual ~ExternalInput();
  virtual bool start() noexcept = 0;
  virtual void stop() noexcept = 0;
};

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

private:
  void buffer_thread() noexcept;
  void close_file() noexcept;
  AVFrame* read_frame_impl() noexcept;
  bool open_stream() noexcept;
  void close_stream() noexcept;
  ReadFrame read_one_frame(AVFramePointer frame, AVPacket& packet);

  static const constexpr int frames_to_buffer = 1;

  AVCodecID m_requestedCodec{AV_CODEC_ID_NONE};
  AVPixelFormat m_requestedPixfmt{AV_PIX_FMT_NONE};

  std::thread m_thread;

  std::string m_inputKind;
  std::string m_inputDevice;

  std::atomic_bool m_running{};
};

}
#endif
