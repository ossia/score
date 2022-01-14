#pragma once
#include <Gfx/Graph/VideoNode.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Video/VideoInterface.hpp>

namespace score::gfx
{
class GPUVideoDecoder;
class VideoNodeRenderer
    : public NodeRenderer
{
public:
  explicit VideoNodeRenderer(const VideoNode& node) noexcept;
  ~VideoNodeRenderer();

  VideoNodeRenderer() = delete;
  VideoNodeRenderer(const VideoNodeRenderer&) = delete;
  VideoNodeRenderer(VideoNodeRenderer&&) = delete;
  VideoNodeRenderer& operator=(const VideoNodeRenderer&) = delete;
  VideoNodeRenderer& operator=(VideoNodeRenderer&&) = delete;

  TextureRenderTarget renderTargetForInput(const Port& input) override;

  void createGpuDecoder();
  void setupGpuDecoder(RenderList& r);
  void checkFormat(RenderList& r, AVPixelFormat fmt, int w, int h);

  void init(RenderList& renderer) override;
  void runRenderPass(
      RenderList&,
      QRhiCommandBuffer& commands,
      Edge& edge) override;

  void update(RenderList& renderer, QRhiResourceUpdateBatch& res) override;
  void release(RenderList& r) override;

private:
  void createPipelines(RenderList& r);
  void displayRealTimeFrame(AVFrame& frame, RenderList& renderer, QRhiResourceUpdateBatch& res);
  void displayVideoFrame(AVFrame& frame, RenderList& renderer, QRhiResourceUpdateBatch& res);
  bool mustReadVideoFrame();
  AVFrame* nextFrame();
  const VideoNode& node;

  std::vector<std::pair<Edge*, Pipeline>> m_p;
  QRhiBuffer* m_meshBuffer{};
  QRhiBuffer* m_idxBuffer{};
  QRhiBuffer* m_processUBO{};
  QRhiBuffer* m_materialUBO{};

  struct Material {
    float scale_w{}, scale_h{};
  };

  std::unique_ptr<GPUVideoDecoder> m_gpu;
  std::shared_ptr<Video::VideoInterface> m_decoder;
  std::vector<AVFrame*> m_framesToFree;

  AVPixelFormat m_currentFormat = AVPixelFormat(-1);
  int m_currentWidth = 0;
  int m_currentHeight = 0;
  score::gfx::ScaleMode m_currentScaleMode{};
  QElapsedTimer m_timer;
  AVFrame* m_nextFrame{};

  double m_lastFrameTime{};
  double m_lastPlaybackTime{-1.};
  bool m_readFrame{};
  bool m_recomputeScale{};
};

}
