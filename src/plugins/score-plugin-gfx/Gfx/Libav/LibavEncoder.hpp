#pragma once

#include <Media/Libav.hpp>
#if SCORE_HAS_LIBAV
#include <Gfx/Libav/LibavOutputSettings.hpp>

#include <ossia/detail/pod_vector.hpp>

#include <mutex>
#include <span>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}
namespace Gfx
{

struct OutputStream;
struct LibavEncoder
{
  explicit LibavEncoder(const LibavOutputSettings& set);
  ~LibavEncoder();
  void enumerate();

  int start();
  int add_frame(std::span<ossia::float_vector>);
  int add_frame(const unsigned char* data, AVPixelFormat fmt, int width, int height);
  // Pre-converted frame: data is already in the target pixel format.
  // planes/strides/planeCount describe the YUV plane layout.
  int add_frame_converted(
      const unsigned char* const planes[], const int strides[], int planeCount,
      int width, int height);
  int stop();
  int stop_impl(); // Must be called with m_muxMutex held

  bool available() const noexcept { return m_formatContext; }

  LibavOutputSettings m_set;
  AVFormatContext* m_formatContext{};

  std::vector<OutputStream> streams;
  std::mutex m_muxMutex; // Protects av_interleaved_write_frame (not thread-safe)

  int audio_stream_index = -1;
  int video_stream_index = -1;
};

}
#endif
