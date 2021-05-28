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

  TextureRenderTarget createRenderTarget(const RenderState& state);

  void init(RenderList& renderer) override;
  void runPass(
      RenderList&,
      QRhiCommandBuffer& commands,
      QRhiResourceUpdateBatch& updateBatch) override;

  void update(RenderList& renderer, QRhiResourceUpdateBatch& res) override;
  void release(RenderList& r) override;
  void releaseWithoutRenderTarget(RenderList&) override;

  TextureRenderTarget renderTarget() const noexcept override;
  std::optional<QSize> renderTargetSize() const noexcept override;

private:
  const VideoNode& node;

  TextureRenderTarget m_rt;
  Pipeline m_p;
  QRhiBuffer* m_meshBuffer{};
  QRhiBuffer* m_idxBuffer{};
  QRhiBuffer* m_processUBO{};


  std::unique_ptr<GPUVideoDecoder> gpu;
  std::shared_ptr<Video::VideoInterface> decoder;
  std::vector<AVFrame*> framesToFree;
  AVPixelFormat current_format = AVPixelFormat(-1);
  int current_width{}, current_height{};
  QElapsedTimer t;

  double lastFrameTime{};
  double lastPlaybackTime{-1.};
};

}
