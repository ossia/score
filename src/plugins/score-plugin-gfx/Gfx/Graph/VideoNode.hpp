#pragma once

#include <Gfx/Graph/Node.hpp>
extern "C"
{
#include <libavformat/avformat.h>
}
#include <atomic>

namespace Video
{
struct VideoInterface;
}
namespace score::gfx
{
class VideoNodeRenderer;
class VideoNode;

struct RefcountedFrame {
  AVFrame* frame{};
  std::atomic_int use_count{};
};

struct VideoFrameReader
{
  ~VideoFrameReader();

  void releaseFramesToFree();
  static AVFrame* nextFrame(const VideoNode& node, Video::VideoInterface& decoder, std::vector<AVFrame*>& framesToFree, AVFrame*& nextFrame);

  std::shared_ptr<RefcountedFrame> currentFrame() const noexcept;
  bool mustReadVideoFrame(const VideoNode& node);
  void readNextFrame(VideoNode& node);
  void updateCurrentFrame(VideoNode& node, AVFrame* frame);

  std::shared_ptr<Video::VideoInterface> m_decoder;
  int64_t m_currentFrameIdx{};

private:
  std::vector<AVFrame*> m_framesToFree;

  mutable std::mutex m_frameLock{};

  std::shared_ptr<RefcountedFrame> m_currentFrame{};


  std::vector<std::shared_ptr<RefcountedFrame>> m_framesInFlight;

  QElapsedTimer m_timer;
  AVFrame* m_nextFrame{};
  double m_lastFrameTime{};
  double m_lastPlaybackTime{-1.};
  bool m_readFrame{};
};

/**
 * @brief Model for rendering a video
 */
struct SCORE_PLUGIN_GFX_EXPORT VideoNode : ProcessNode
{
public:
  VideoNode(
      std::shared_ptr<Video::VideoInterface> dec,
      std::optional<double> nativeTempo,
      QString f = {});

  virtual ~VideoNode();

  score::gfx::NodeRenderer*
  createRenderer(RenderList& r) const noexcept override;

  void seeked();

  void setScaleMode(score::gfx::ScaleMode s);
  void process(const Message& msg) override;

  VideoFrameReader reader;
private:
  friend VideoFrameReader;
  friend VideoNodeRenderer;

  QString m_filter;
  std::optional<double> m_nativeTempo;
  score::gfx::ScaleMode m_scaleMode{};
};

}
