#pragma once

#include <Gfx/Graph/Node.hpp>
#include <Video/VideoEnums.hpp>
extern "C" {
#include <libavformat/avformat.h>
}
#include <ossia/detail/mutex.hpp>

#include <atomic>
#include <limits>

namespace Video
{
struct VideoInterface;
class ExternalInput;
}
namespace score::gfx
{
class VideoNodeRenderer;
class DirectVideoNodeRenderer;
class VideoNode;

struct RefcountedFrame
{
  AVFrame* frame{};
  std::atomic_int use_count{};
};

struct SCORE_PLUGIN_GFX_EXPORT VideoFrameShare
{
  VideoFrameShare();
  ~VideoFrameShare();

  std::shared_ptr<RefcountedFrame> currentFrame() const noexcept;
  void updateCurrentFrame(AVFrame* frame);
  void releaseFramesToFree();
  void releaseAllFrames();

  std::shared_ptr<Video::VideoInterface> m_decoder;

  mutable std::mutex m_frameLock{};
  std::shared_ptr<RefcountedFrame> m_currentFrame TS_GUARDED_BY(m_frameLock);

  int64_t m_currentFrameIdx{};

  std::vector<AVFrame*> m_framesToFree;
  std::vector<std::shared_ptr<RefcountedFrame>> m_framesInFlight;
};

struct SCORE_PLUGIN_GFX_EXPORT VideoFrameReader : VideoFrameShare
{
  VideoFrameReader();
  ~VideoFrameReader();

  /// The frame for the node's current time, if a new one is due.
  /// @param showing whether a frame is on screen: until one is, the first
  ///        frame shows even before its time, as in DirectVideoNodeRenderer.
  static AVFrame* nextFrame(
      const VideoNode& node, Video::VideoInterface& decoder,
      std::vector<AVFrame*>& framesToFree, AVFrame*& nextFrame, bool showing);

  bool mustReadVideoFrame(const VideoNode& node);
  void readNextFrame(VideoNode& node);

  void pause(bool p);

private:
  QElapsedTimer m_timer;
  AVFrame* m_nextFrame{};
  double m_lastFrameTime{};
  double m_lastPlaybackTime{-1.};
  bool m_readFrame{};
  bool m_showing{};
  //! The seek generation a backward seek was last asked at: none other is
  //! asked until the decoder has done it.
  int m_backwardSeekGeneration{-1};
  double m_earliestFrameTime{std::numeric_limits<double>::infinity()};
};

class SCORE_PLUGIN_GFX_EXPORT VideoNodeBase : public ProcessNode
{
public:
  void setScaleMode(score::gfx::ScaleMode s);
  void setPlaybackMode(score::gfx::PlaybackMode s);
  void setOutputFormat(::Video::OutputFormat s);
  void setTonemap(::Video::Tonemap s);
  void setDecodingSettings(QString hardwareDecode, int threads);

  friend VideoNodeRenderer;
  friend DirectVideoNodeRenderer;

protected:
  QString m_filter;
  score::gfx::ScaleMode m_scaleMode{};
  score::gfx::PlaybackMode m_playbackMode{};
  ::Video::OutputFormat m_outputFormat{};
  ::Video::Tonemap m_tonemap{};
  //! The "Hardware Video Decoding" and "Decoding threads" settings, for the
  //! renderers that open their own decoder. Empty means no hardware decoding.
  QString m_hardwareDecode;
  int m_decodingThreads{};
};

/**
 * @brief Model for rendering a video
 */
class SCORE_PLUGIN_GFX_EXPORT VideoNode : public VideoNodeBase
{
public:
  VideoNode(
      std::shared_ptr<Video::VideoInterface> dec, std::optional<double> nativeTempo);

  virtual ~VideoNode();

  score::gfx::NodeRenderer* createRenderer(RenderList& r) const noexcept override;

  void seeked();

  void process(Message&& msg) override;
  void update() override;

  VideoFrameReader reader;

  void pause(bool);

private:
  friend VideoFrameReader;
  friend VideoNodeRenderer;

  std::optional<double> m_nativeTempo;
  Timings m_lastToken{};
  QElapsedTimer m_timer;
  //! Playback seconds per wall-clock second, between the last two messages.
  double m_rate{1.};
  std::atomic_bool m_pause{};
};
}

namespace score::gfx
{
/**
 * @brief Model for rendering a camera feed
 */
class SCORE_PLUGIN_GFX_EXPORT CameraNode : public VideoNodeBase
{
public:
  explicit CameraNode(std::shared_ptr<Video::ExternalInput> dec, QString filter = {});

  virtual ~CameraNode();

  score::gfx::NodeRenderer* createRenderer(RenderList& r) const noexcept override;

  void process(Message&& msg) override;
  void renderedNodesChanged() override;

  VideoFrameShare reader;

  std::atomic_bool must_stop{};

private:
  friend VideoNodeRenderer;
};

}
