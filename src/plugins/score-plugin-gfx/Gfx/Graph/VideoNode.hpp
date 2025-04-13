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
class ExternalInput;
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

  static AVFrame* nextFrame(
      const VideoNode& node, Video::VideoInterface& decoder,
      std::vector<AVFrame*>& framesToFree, AVFrame*& nextFrame);

  bool mustReadVideoFrame(const VideoNode& node);
  void readNextFrame(VideoNode& node);

  void pause(bool p);

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

/**
 * @brief Model for getting more general streams of data e.g. point clouds
 */
class SCORE_PLUGIN_GFX_EXPORT BufferNode : public Node
{
public:
  explicit BufferNode(std::shared_ptr<Video::ExternalInput> dec);
  ~BufferNode() { }

  NodeRenderer* createRenderer(RenderList& r) const noexcept override;

  void process(Message&& msg) override;
  void renderedNodesChanged() override;

  VideoFrameShare reader;
  std::atomic_bool must_stop{};
};
}
