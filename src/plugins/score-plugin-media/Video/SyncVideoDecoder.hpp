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

#include <score_plugin_media_export.h>

#include <string>

namespace Video
{
/**
 * @brief Synchronous video decoder for frame-accurate playback
 *
 * Unlike VideoDecoder which uses a background thread and buffer queue,
 * SyncVideoDecoder decodes frames on-demand when requested. This provides:
 * - Zero latency seeking (immediate frame-accurate positioning)
 * - Precise synchronization with external timing sources
 * - No buffer delay when stopping/starting playback
 *
 * Trade-offs:
 * - Decode happens on the calling thread (may block if slow)
 * - No read-ahead buffering
 */
class SCORE_PLUGIN_MEDIA_EXPORT SyncVideoDecoder final
    : public VideoInterface
    , public LibAVDecoder
{
public:
  explicit SyncVideoDecoder(DecoderConfiguration conf) noexcept;
  ~SyncVideoDecoder() noexcept;

  bool open(const std::string& inputFile) noexcept;

  const std::string& file() const noexcept { return m_inputFile; }

  int64_t duration() const noexcept;

  // VideoInterface - sync mode
  DecodingMode decodingMode() const noexcept override { return DecodingMode::Sync; }
  void seek(int64_t flicks) override;
  AVFrame* decode_frame_at(int64_t flicks) noexcept override;

  // VideoInterface - for compatibility (returns cached frame)
  AVFrame* dequeue_frame() noexcept override;
  void release_frame(AVFrame* frame) noexcept override;

private:
  bool seek_impl(int64_t flicks) noexcept;
  AVFrame* read_frame_impl() noexcept;
  bool open_stream() noexcept;
  void close_file() noexcept;
  void close_video() noexcept;

  bool is_frame_at_time(AVFrame* frame, int64_t target_flicks) const noexcept;
  bool needs_seek(int64_t target_flicks) const noexcept;

  std::string m_inputFile;
  int64_t m_duration{}; // in flicks

  // Cached last decoded frame for dequeue_frame() compatibility
  AVFrame* m_lastFrame{};
  int64_t m_lastFrameFlicks{-1};

  // Current position tracking
  int64_t m_currentPosition{0};
};

}
#endif
