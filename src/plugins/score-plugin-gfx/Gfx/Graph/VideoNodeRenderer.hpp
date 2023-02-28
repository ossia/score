#pragma once
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/VideoNode.hpp>
#include <Video/VideoInterface.hpp>

namespace score::gfx
{
class GPUVideoDecoder;

class VideoNodeRenderer : public NodeRenderer
{
public:
  explicit VideoNodeRenderer(
      const VideoNodeBase& node, VideoFrameShare& frames) noexcept;
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

  void init(RenderList& renderer, QRhiResourceUpdateBatch& res) override;
  void runRenderPass(RenderList&, QRhiCommandBuffer& commands, Edge& edge) override;

  void update(RenderList& renderer, QRhiResourceUpdateBatch& res) override;
  void release(RenderList& r) override;

private:
  void createPipelines(RenderList& r);
  void displayFrame(AVFrame& frame, RenderList& renderer, QRhiResourceUpdateBatch& res);
  Video::VideoMetadata& decoder() const noexcept;

  const VideoNodeBase& node;
  VideoFrameShare& reader;

  PassMap m_p;
  QRhiBuffer* m_meshBuffer{};
  QRhiBuffer* m_idxBuffer{};
  QRhiBuffer* m_processUBO{};
  QRhiBuffer* m_materialUBO{};

  struct Material
  {
    float scale_w{}, scale_h{};
  };

  std::unique_ptr<GPUVideoDecoder> m_gpu;

  Video::ImageFormat m_frameFormat{};
  score::gfx::ScaleMode m_currentScaleMode{};

  std::shared_ptr<RefcountedFrame> m_currentFrame{};
  int64_t m_currentFrameIdx{-1};
  bool m_recomputeScale{};
};

}
