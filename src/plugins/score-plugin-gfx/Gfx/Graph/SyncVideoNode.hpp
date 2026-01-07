#pragma once

#include <Gfx/Graph/VideoNode.hpp>

namespace Video
{
class SyncVideoDecoder;
}

namespace score::gfx
{

/**
 * @brief Frame reader for synchronous video decoding
 *
 * Unlike VideoFrameReader which manages async queue polling and drift correction,
 * SyncVideoFrameReader directly requests frames at specific times from the decoder.
 * This provides frame-accurate positioning with zero latency.
 */
struct SyncVideoFrameReader : VideoFrameShare
{
  SyncVideoFrameReader();
  ~SyncVideoFrameReader();

  void readNextFrame(double currentTime);

private:
  int64_t m_lastRequestedFlicks{-1};
};

/**
 * @brief Video node for synchronous (frame-accurate) video playback
 *
 * This node uses SyncVideoDecoder to decode frames on-demand, providing:
 * - Zero-latency seeking
 * - Frame-accurate synchronization
 * - No buffer delay
 *
 * Use this when precise timing is critical (e.g., syncing with audio,
 * external timecode, or interactive control).
 */
class SCORE_PLUGIN_GFX_EXPORT SyncVideoNode : public VideoNodeBase
{
public:
  SyncVideoNode(
      std::shared_ptr<Video::VideoInterface> dec, std::optional<double> nativeTempo);

  virtual ~SyncVideoNode();

  score::gfx::NodeRenderer* createRenderer(RenderList& r) const noexcept override;

  void process(Message&& msg) override;
  void update() override;

  SyncVideoFrameReader reader;

  void pause(bool);

private:
  friend SyncVideoFrameReader;
  friend VideoNodeRenderer;

  std::optional<double> m_nativeTempo;
  Timings m_lastToken{};
  QElapsedTimer m_timer;
  std::atomic_bool m_pause{};
};

}
