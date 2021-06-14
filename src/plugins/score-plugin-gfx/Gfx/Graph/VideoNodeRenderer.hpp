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
  VideoNodeRenderer(const VideoNode& node) noexcept;

  ~VideoNodeRenderer();

  void createGpuDecoder();
  void setupGpuDecoder(RenderList& r);
  void checkFormat(RenderList& r, AVPixelFormat fmt, int w, int h);


  void init(RenderList& renderer) override;
  void runPass(
      RenderList&,
      QRhiCommandBuffer& commands,
      QRhiResourceUpdateBatch& updateBatch) override;

  void update(RenderList& renderer, QRhiResourceUpdateBatch& res) override;
  void release(RenderList& r) override;
  void releaseWithoutRenderTarget(RenderList&) override;

  //TextureRenderTarget createRenderTarget(const RenderState& state);
  //TextureRenderTarget renderTarget() const noexcept override;
  //std::optional<QSize> renderTargetSize() const noexcept override;

private:
  const VideoNode& node;

  TextureRenderTarget m_rt;
  Pipeline m_p;
  QRhiBuffer* m_meshBuffer{};
  QRhiBuffer* m_idxBuffer{};
  QRhiBuffer* m_processUBO{};


  std::unique_ptr<GPUVideoDecoder> m_gpu;
  std::shared_ptr<Video::VideoInterface> m_decoder;
  std::vector<AVFrame*> m_framesToFree;
  AVPixelFormat m_currentFormat = AVPixelFormat(-1);
  int m_currentWidth = 0;
  int m_currentHeight = 0;
  QElapsedTimer m_timer;

  double m_lastFrameTime{};
  double m_lastPlaybackTime{-1.};
};

}
