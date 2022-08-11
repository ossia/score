#pragma once

#include <Gfx/Graph/Node.hpp>
extern "C" {
#include <libavformat/avformat.h>
}
#include <ossia/detail/mutex.hpp>

#include <atomic>

namespace Video
{
struct VideoInterface;
}
namespace score::gfx
{
class VideoNodeRenderer;
class VideoNode;

struct RefcountedFrame
{
  AVFrame* frame{};
  std::atomic_int use_count{};
};

struct VideoFrameShare
{
  VideoFrameShare();
  ~VideoFrameShare();

  std::shared_ptr<RefcountedFrame> currentFrame() const noexcept;
  void updateCurrentFrame(AVFrame* frame);
  void releaseFramesToFree();

  std::shared_ptr<Video::VideoInterface> m_decoder;

  mutable std::mutex m_frameLock{};
  std::shared_ptr<RefcountedFrame> m_currentFrame TS_GUARDED_BY(m_frameLock);

  int64_t m_currentFrameIdx{};

  std::vector<AVFrame*> m_framesToFree;
  std::vector<std::shared_ptr<RefcountedFrame>> m_framesInFlight;
};

struct VideoFrameReader : VideoFrameShare
{
  VideoFrameReader();
  ~VideoFrameReader();

  static AVFrame* nextFrame(
      const VideoNode& node, Video::VideoInterface& decoder,
      std::vector<AVFrame*>& framesToFree, AVFrame*& nextFrame);

  bool mustReadVideoFrame(const VideoNode& node);
  void readNextFrame(VideoNode& node);

private:
  QElapsedTimer m_timer;
  AVFrame* m_nextFrame{};
  double m_lastFrameTime{};
  double m_lastPlaybackTime{-1.};
  bool m_readFrame{};
};

class SCORE_PLUGIN_GFX_EXPORT VideoNodeBase : public ProcessNode
{
public:
  void setScaleMode(score::gfx::ScaleMode s);

  friend VideoNodeRenderer;

protected:
  QString m_filter;
  score::gfx::ScaleMode m_scaleMode{};
};

/**
 * @brief Model for rendering a video
 */
class SCORE_PLUGIN_GFX_EXPORT VideoNode : public VideoNodeBase
{
public:
  VideoNode(
      std::shared_ptr<Video::VideoInterface> dec, std::optional<double> nativeTempo,
      QString f = {});

  virtual ~VideoNode();

  score::gfx::NodeRenderer* createRenderer(RenderList& r) const noexcept override;

  void seeked();

  void process(const Message& msg) override;

  VideoFrameReader reader;

private:
  friend VideoFrameReader;
  friend VideoNodeRenderer;

  std::optional<double> m_nativeTempo;
};

/**
 * @brief Model for rendering a camera feed
 */
class SCORE_PLUGIN_GFX_EXPORT CameraNode : public VideoNodeBase
{
public:
  CameraNode(std::shared_ptr<Video::VideoInterface> dec, QString f = {});

  virtual ~CameraNode();

  score::gfx::NodeRenderer* createRenderer(RenderList& r) const noexcept override;

  void process(const Message& msg) override;

  VideoFrameShare reader;

private:
  friend VideoNodeRenderer;

  std::shared_ptr<RefcountedFrame> m_currentFrame{};
};

}
